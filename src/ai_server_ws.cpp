#include "ai_server_ws.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <freertos/queue.h>
#include <esp_heap_caps.h>
#include "secrets.h"
#include "net_reconnect.h"
#include "motor.h"
#include "settings.h"
#include "ondevice_ai.h"
#include "net_log.h"

using namespace websockets;

static WebsocketsClient ai_ws_client;

static const uint32_t AI_WS_RECONNECT_INTERVAL_MS = 3000;

static const size_t PACKET_SIZE = 4 + SAMPLE_RATE * sizeof(int16_t); // [1바이트 방향][1바이트 온디바이스 판정][2바이트 패딩][PCM int16 데이터]

// core 1이 채워서 core 0로 넘길 이중 버퍼. busy 중엔 재사용 안 함.
// core 1은 매 사이클 ai_ws_get_pcm_buf()로 빈 슬롯을 얻어 PCM을 직접 써넣는다(중간 스크래치 버퍼 없음).
// PSRAM에 할당(ai_ws_start에서) — 오디오 전송은 0.5초에 한 번뿐이라 PSRAM 지연이 문제되지 않음.
static uint8_t* net_bufs[2] = { nullptr, nullptr };
static volatile bool net_buf_busy[2] = { false, false };

// ai_ws_get_pcm_buf()가 고른, 아직 ai_ws_send()로 커밋되지 않은 슬롯. -1이면 없음.
// net_buf_busy는 ai_ws_send()만 true로 바꾼다(get_pcm_buf는 읽기만 함) — 호출 짝이 깨져도 슬롯이 영구 busy로 안 남게.
static int pending_idx = -1;

struct AudioPacket {
    uint8_t idx;
    size_t  len;
};
static QueueHandle_t audio_queue;  // 전송 대기열: ai_ws_task가 꺼내서 AI서버로 전송
static volatile bool ai_ws_connected = false;  // ai_ws_task가 갱신
#if ONDEVICE_AI_ENABLED
static QueueHandle_t infer_queue;  // 추론 대기열: ondevice_task가 꺼내서 판정 후 audio_queue로 넘김
#endif

static String build_ws_url() {
    return String("ws://") + websockets_server_host + ":" + websockets_server_port + "/ws/neckband";
}

// core 0 전용 태스크: WiFi/웹소켓 재연결 + 오디오 전송. core 1 오디오 캡처와 완전히 분리.
static void ai_ws_task(void* param) {
    ai_ws_client.onMessage([](WebsocketsMessage msg) {
        Serial.println(msg.data());
    });

    uint32_t last_ws_reconnect_ms = 0;
    AudioPacket pkt;

    for (;;) {
        bool connected = wifi_ensure_connected() &&
            ws_ensure_connected(ai_ws_client, build_ws_url, "AI서버", AI_WS_RECONNECT_INTERVAL_MS, last_ws_reconnect_ms);
        if (connected) {
            ai_ws_client.poll();
        }
        ai_ws_connected = connected;

        if (xQueueReceive(audio_queue, &pkt, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (ai_ws_client.available()) {
                ai_ws_client.sendBinary((const char*)net_bufs[pkt.idx], pkt.len);
            }
            net_buf_busy[pkt.idx] = false;
        }
    }
}

#if ONDEVICE_AI_ENABLED
// core 0 전용 태스크: 온디바이스 AI로 먼저 판정하고 결과 플래그를 패킷에 넣어 ai_ws_task로 넘김.
// 전송 태스크와 분리해 추론 중에도 웹소켓 poll/재연결이 멈추지 않게 함.
static void ondevice_task(void* param) {
    // 모델 로딩은 스택 여유가 있는 이 태스크에서 함. 실패해도 패킷은 계속 전달(온디바이스만 꺼진 상태).
    bool ready = ondevice_ai_init();
    if (!ready) {
        Serial.println("[온디바이스AI] 사용 불가, AI서버 전송만 계속");
    }

    AudioPacket pkt;
    uint32_t count = 0;

    for (;;) {
        xQueueReceive(infer_queue, &pkt, portMAX_DELAY);

        uint8_t* buf = net_bufs[pkt.idx];
        uint8_t judged = 0;  // 패킷 [1]번 바이트: 1이면 온디바이스가 긴급으로 판정

        // 버튼 꺼짐/방해금지 중엔 추론 자체를 스킵.
        if (ready && settings_emergency_alert_enabled() && !settings_do_not_disturb()) {
            Direction dir = (Direction)buf[0];
            uint32_t feature_us = 0, infer_us = 0;
            float prob = ondevice_ai_infer((const int16_t*)(buf + 4), &feature_us, &infer_us);

            if (prob >= 0.0f) {
                judged = (prob >= ONDEVICE_AI_THRESHOLD) ? 1 : 0;
                Serial.printf("[온디바이스AI] 긴급확률=%.3f (%s) 방향=%s | 특징 %u us, 추론 %u us\n",
                              prob, judged ? "긴급" : "일반", direction_to_str(dir),
                              (unsigned)feature_us, (unsigned)infer_us);
                if (judged) {
                    motor_vibrate(dir, settings_haptic_strength());
                }
            }
        }
        buf[1] = judged;

        // 미연결이면 버퍼 바로 반납 (접속 시도 블로킹에 버퍼가 묶여 추론이 멈추는 것 방지)
        if (!ai_ws_connected) {
            net_buf_busy[pkt.idx] = false;
        } else if (xQueueSend(audio_queue, &pkt, 0) != pdTRUE) {
            net_buf_busy[pkt.idx] = false;
            Serial.println("AI서버 전송 큐 가득 참, 오디오 블록 드롭");
        }

        // 스택 여유 확인용: 20패킷마다 최소 남은 양 출력
        if (++count % 20 == 0) {
            Serial.printf("[온디바이스AI] 스택 최소 남은 양: %u 바이트\n",
                          (unsigned)uxTaskGetStackHighWaterMark(NULL));
        }
    }
}
#endif

void ai_ws_start() {
    for (int i = 0; i < 2; i++) {
        net_bufs[i] = (uint8_t*)heap_caps_malloc(PACKET_SIZE, MALLOC_CAP_SPIRAM);
        if (net_bufs[i] == nullptr) {
            Serial.println("PSRAM 전송 버퍼 할당 실패");
        }
    }
    WiFi.begin(ssid, password);
    audio_queue = xQueueCreate(2, sizeof(AudioPacket));
#if ONDEVICE_AI_ENABLED
    infer_queue = xQueueCreate(2, sizeof(AudioPacket));
    xTaskCreatePinnedToCore(ondevice_task, "ondevice_ai", 16384, NULL, 1, NULL, 0);
#endif
    xTaskCreatePinnedToCore(ai_ws_task, "ai_ws", 8192, NULL, 1, NULL, 0);
}

// core 1에서 호출. 빈 슬롯이 없으면 nullptr 반환 — 이때 호출부는 audio_flatten/ai_ws_send를 건너뛰어야 함.
int16_t* ai_ws_get_pcm_buf() {
    if (net_bufs[0] != nullptr && !net_buf_busy[0])      pending_idx = 0;
    else if (net_bufs[1] != nullptr && !net_buf_busy[1]) pending_idx = 1;
    else {
        pending_idx = -1;
        Serial.println("AI서버 전송 버퍼 가득 참, 오디오 블록 드롭");
        return nullptr;
    }
    return (int16_t*)(net_bufs[pending_idx] + 4);
}

// core 1에서 호출. 블로킹 금지. ai_ws_get_pcm_buf()가 nullptr을 반환했을 땐 호출하지 말 것(방어적으로 그래도 no-op).
void ai_ws_send(Direction dir, int num_samples) {
    if (pending_idx < 0) return;
    int idx = pending_idx;
    pending_idx = -1;

    net_bufs[idx][0] = static_cast<uint8_t>(dir);
    net_bufs[idx][1] = 0;
    net_bufs[idx][2] = 0;
    net_bufs[idx][3] = 0;

    net_buf_busy[idx] = true;
    size_t len = 4 + num_samples * sizeof(int16_t);

    AudioPacket pkt{ (uint8_t)idx, len };
#if ONDEVICE_AI_ENABLED
    QueueHandle_t next_queue = infer_queue;   // 온디바이스 판정을 먼저 거침
#else
    QueueHandle_t next_queue = audio_queue;   // 바로 전송
#endif
    if (xQueueSend(next_queue, &pkt, 0) != pdTRUE) {
        net_buf_busy[idx] = false;
        Serial.println("AI서버 전송 큐 가득 참, 오디오 블록 드롭");
    }
}
