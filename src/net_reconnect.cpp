#include "net_reconnect.h"
#include <WiFi.h>

static const uint32_t WIFI_RECONNECT_INTERVAL_MS = 3000;
static uint32_t last_wifi_reconnect_ms = 0;
static portMUX_TYPE wifi_reconnect_mux = portMUX_INITIALIZER_UNLOCKED;

bool wifi_ensure_connected() {
    if (WiFi.status() == WL_CONNECTED) return true;

    uint32_t now = millis();
    bool should_reconnect = false;

    // 체크+갱신을 원자적으로 묶어 두 태스크가 동시에 통과하는 것 방지.
    portENTER_CRITICAL(&wifi_reconnect_mux);
    if (now - last_wifi_reconnect_ms >= WIFI_RECONNECT_INTERVAL_MS) {
        last_wifi_reconnect_ms = now;
        should_reconnect = true;
    }
    portEXIT_CRITICAL(&wifi_reconnect_mux);

    if (should_reconnect) {
        Serial.println("WiFi 연결 시도...");
        WiFi.reconnect();
    }
    return false;
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
