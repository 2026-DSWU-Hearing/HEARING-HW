#include "net_reconnect.h"
#include <WiFi.h>

static const uint32_t WIFI_RECONNECT_INTERVAL_MS = 3000;
static uint32_t last_wifi_reconnect_ms = 0;

void wifi_ensure_connected() {
    if (WiFi.status() == WL_CONNECTED) return;
    uint32_t now = millis();
    if (now - last_wifi_reconnect_ms < WIFI_RECONNECT_INTERVAL_MS) return;
    last_wifi_reconnect_ms = now;
    Serial.println("WiFi 연결 시도...");
    WiFi.reconnect();
}

bool ws_ensure_connected(websockets::WebsocketsClient& client, String (*url_builder)(),
                          const char* label, uint32_t interval_ms, uint32_t& last_attempt_ms) {
    if (client.available()) return true;
    uint32_t now = millis();
    if (now - last_attempt_ms < interval_ms) return false;
    last_attempt_ms = now;
    Serial.printf("%s 웹소켓 연결 시도...\n", label);
    if (client.connect(url_builder())) {
        Serial.printf("%s 웹소켓 연결됨\n", label);
        return true;
    }
    Serial.printf("%s 웹소켓 연결 실패, 재시도 예정\n", label);
    return false;
}
