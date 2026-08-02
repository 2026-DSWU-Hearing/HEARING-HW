#include "ai_server_ws.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <freertos/queue.h>
#include "secrets.h"

using namespace websockets;

static WebsocketsClient ai_ws_client;

static const uint32_t WIFI_RECONNECT_INTERVAL_MS  = 3000;
static const uint32_t AI_WS_RECONNECT_INTERVAL_MS = 3000;

static const size_t PACKET_SIZE = 4 + SAMPLE_RATE * sizeof(int16_t); // [1바이트 방향][3바이트 패딩][PCM int16 데이터]

static uint8_t send_buf[PACKET_SIZE]; // core 1 오디오 캡처 루프가 채워 넣는 스크래치 버퍼

// core 0로 넘길 이중 버퍼. busy 중엔 재사용 안 함.
static uint8_t net_bufs[2][PACKET_SIZE];
static volatile bool net_buf_busy[2] = { false, false };

struct AudioPacket {
    uint8_t idx;
    size_t  len;
};
static QueueHandle_t audio_queue;

static String build_ws_url() {
    return String("ws://") + websockets_server_host + ":" + websockets_server_port + "/ws";
}

// core 0 전용 태스크: WiFi/웹소켓 재연결 + 오디오 전송. core 1 오디오 캡처와 완전히 분리.
static void ai_ws_task(void* param) {
    ai_ws_client.onMessage([](WebsocketsMessage msg) {
        Serial.println(msg.data());
    });

    uint32_t last_wifi_reconnect_ms = 0;
    uint32_t last_ws_reconnect_ms   = 0;
    AudioPacket pkt;

    for (;;) {
        uint32_t now = millis();

        if (WiFi.status() != WL_CONNECTED) {
            if (now - last_wifi_reconnect_ms >= WIFI_RECONNECT_INTERVAL_MS) {
                last_wifi_reconnect_ms = now;
                Serial.println("WiFi 연결 시도...");
                WiFi.reconnect();
            }
        } else if (!ai_ws_client.available()) {
            if (now - last_ws_reconnect_ms >= AI_WS_RECONNECT_INTERVAL_MS) {
                last_ws_reconnect_ms = now;
                Serial.println("AI서버 웹소켓 연결 시도...");
                if (ai_ws_client.connect(build_ws_url())) {
                    Serial.println("AI서버 웹소켓 연결됨");
                } else {
                    Serial.println("AI서버 웹소켓 연결 실패, 재시도 예정");
                }
            }
        } else {
            ai_ws_client.poll();
        }

        if (xQueueReceive(audio_queue, &pkt, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (ai_ws_client.available()) {
                ai_ws_client.sendBinary((const char*)net_bufs[pkt.idx], pkt.len);
            }
            net_buf_busy[pkt.idx] = false;
        }
    }
}

void ai_ws_start() {
    WiFi.begin(ssid, password);
    audio_queue = xQueueCreate(2, sizeof(AudioPacket));
    xTaskCreatePinnedToCore(ai_ws_task, "ai_ws", 8192, NULL, 1, NULL, 0);
}

int16_t* ai_ws_get_pcm_buf() {
    return (int16_t*)(send_buf + 4);
}

// core 1에서 호출. 블로킹 금지 — 보낼 버퍼가 없으면 즉시 드롭.
void ai_ws_send(Direction dir, int num_samples) {
    send_buf[0] = static_cast<uint8_t>(dir);
    send_buf[1] = 0;
    send_buf[2] = 0;
    send_buf[3] = 0;

    int idx = -1;
    if (!net_buf_busy[0])      idx = 0;
    else if (!net_buf_busy[1]) idx = 1;
    if (idx < 0) {
        Serial.println("AI서버 전송 버퍼 가득 참, 오디오 블록 드롭");
        return;
    }

    size_t len = 4 + num_samples * sizeof(int16_t);
    net_buf_busy[idx] = true;
    memcpy(net_bufs[idx], send_buf, len);

    AudioPacket pkt{ (uint8_t)idx, len };
    if (xQueueSend(audio_queue, &pkt, 0) != pdTRUE) {
        net_buf_busy[idx] = false;
        Serial.println("AI서버 전송 큐 가득 참, 오디오 블록 드롭");
    }
}
