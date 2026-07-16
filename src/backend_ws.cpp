#include "backend_ws.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "motor.h"
#include "direction.h"

using namespace websockets;

static WebsocketsClient backend_ws_client;

static const uint32_t RECONNECT_INTERVAL_MS = 3000;
static const uint32_t STATUS_INTERVAL_MS    = 30000;

static Direction parse_direction(const char* s) {
    if (s == nullptr)              return Direction::UNKNOWN;
    if (strcmp(s, "FRONT") == 0)   return Direction::FRONT;
    if (strcmp(s, "BACK")  == 0)   return Direction::BACK;
    if (strcmp(s, "LEFT")  == 0)   return Direction::LEFT;
    if (strcmp(s, "RIGHT") == 0)   return Direction::RIGHT;
    return Direction::UNKNOWN;
}

static void on_message(WebsocketsMessage msg) {
    JsonDocument doc;
    if (deserializeJson(doc, msg.data()) != DeserializationError::Ok) {
        Serial.println("백엔드 메시지 JSON 파싱 실패");
        return;
    }

    const char* type = doc["type"];
    if (type == nullptr) return;

    if (strcmp(type, "vibrate") == 0) {
        int strength = doc["strength"] | 0;
        strength = constrain(strength, 0, 100);
        // TODO: 백엔드 direction 필드 추가 전까지는 항상 UNKNOWN으로 들어와서 진동 안 울림.
        Direction dir = parse_direction(doc["direction"] | (const char*)nullptr);
        Serial.printf("vibrate 수신: strength=%d direction=%s\n", strength, direction_to_str(dir));
        motor_vibrate(dir, (uint8_t)strength);
    }
}

// 재연결 시도. 성공 시 콜백 등록은 connect() 이전에 이미 되어 있어야 함.
static bool try_connect() {
    String url = String("ws://") + backend_ws_host + ":" + backend_ws_port +
                 "/ws/devices?token=" + backend_device_token +
                 "&mac=" + WiFi.macAddress();
    return backend_ws_client.connect(url);
}

static void send_status() {
    JsonDocument doc;
    doc["type"] = "status";
    doc["battery_level"] = 100;   // TODO: 실제 배터리 ADC 연결 후 계산값으로 교체
    doc["connection_type"] = "wifi";
    String out;
    serializeJson(doc, out);
    backend_ws_client.send(out);
}

static void backend_task(void* param) {
    backend_ws_client.onMessage(on_message);

    uint32_t last_reconnect_attempt_ms = 0;
    uint32_t last_status_ms = 0;

    for (;;) {
        if (!backend_ws_client.available()) {
            uint32_t now = millis();
            if (now - last_reconnect_attempt_ms >= RECONNECT_INTERVAL_MS) {
                last_reconnect_attempt_ms = now;
                Serial.println("백엔드 소켓 연결 시도...");
                if (try_connect()) {
                    Serial.println("백엔드 소켓 연결됨");
                } else {
                    Serial.println("백엔드 소켓 연결 실패, 재시도 예정");
                }
            }
        } else {
            backend_ws_client.poll();

            uint32_t now = millis();
            if (now - last_status_ms >= STATUS_INTERVAL_MS) {
                last_status_ms = now;
                send_status();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void backend_ws_start() {
    xTaskCreatePinnedToCore(backend_task, "backend_ws", 8192, NULL, 1, NULL, 0);
}
