#include "backend_ws.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "motor.h"
#include "direction.h"
#include "battery.h"
#include "net_reconnect.h"

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
        Direction dir = parse_direction(doc["direction"] | (const char*)nullptr);
        Serial.printf("vibrate 수신: strength=%d direction=%s\n", strength, direction_to_str(dir));
        motor_vibrate(dir, (uint8_t)strength);
    }
}

static String build_backend_url() {
    return String("ws://") + backend_ws_host + ":" + backend_ws_port +
                 "/ws/devices?token=" + backend_device_token +
                 "&mac=" + WiFi.macAddress();
}

static void send_status() {
    JsonDocument doc;
    doc["type"] = "status";
    doc["battery_level"] = battery_get_percent();
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
        if (wifi_ensure_connected() &&
            ws_ensure_connected(backend_ws_client, build_backend_url, "백엔드", RECONNECT_INTERVAL_MS, last_reconnect_attempt_ms)) {
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
