#include "net_reconnect.h"
#include <WiFi.h>
#include <time.h>
#include "ca_cert.h"
#include "net_log.h"

static const uint32_t WIFI_RECONNECT_INTERVAL_MS = 3000;
static const time_t   TIME_SYNCED_MIN_EPOCH      = 1700000000; // 2023-11 이후면 NTP 동기화된 것으로 봄
static uint32_t last_wifi_reconnect_ms = 0;
static portMUX_TYPE wifi_reconnect_mux = portMUX_INITIALIZER_UNLOCKED;
static bool ntp_started = false;

static bool is_wss(const String& url) { return url.startsWith("wss://"); }
static bool time_synced()             { return time(nullptr) > TIME_SYNCED_MIN_EPOCH; }

bool wifi_ensure_connected() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!ntp_started) {  // 두 태스크가 겹쳐 두 번 불려도 무해
            ntp_started = true;
            configTime(0, 0, "pool.ntp.org", "time.google.com");
        }
        return true;
    }

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

void ws_setup_tls(websockets::WebsocketsClient& client, const char* url) {
    if (is_wss(url)) client.setCACert(LETSENCRYPT_ROOT_CA);
}

bool ws_ensure_connected(websockets::WebsocketsClient& client, String (*url_builder)(),
                          const char* label, uint32_t interval_ms, uint32_t& last_attempt_ms) {
    if (client.available()) return true;
    uint32_t now = millis();
    if (now - last_attempt_ms < interval_ms) return false;
    last_attempt_ms = now;

    String url = url_builder();
    if (is_wss(url) && !time_synced()) {  // 시계가 1970년이면 인증서가 "아직 유효하지 않음"으로 거부됨
        Serial.printf("%s 웹소켓: NTP 시간 동기화 대기 중\n", label);
        return false;
    }
    Serial.printf("%s 웹소켓 연결 시도...\n", label);
    if (client.connect(url)) {
        Serial.printf("%s 웹소켓 연결됨\n", label);
        return true;
    }
    Serial.printf("%s 웹소켓 연결 실패, 재시도 예정\n", label);
    return false;
}
