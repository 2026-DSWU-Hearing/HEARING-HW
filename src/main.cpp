#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "audio.h"
#include "ai_server_ws.h"
#include "backend_ws.h"
#include "motor.h"
#include "battery.h"
#include "led.h"
#include "detector.h"
#include "net_log.h"
#include "net_reconnect.h"
#include "ota.h"
#include "config.h"
#if DEBUG_METRICS_ENABLED
#include <esp_heap_caps.h>
#endif

void setup() {
    Serial.begin(115200);

    // DEBUG: 주변 와이파이 스캔 (부팅 시 신호 확인용). 블로킹 호출이라 LED 초기화를 지연시켜 주석 처리.
    /*
    Serial.println("\n주변 와이파이 검색 중...");
    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.println("검색된 와이파이가 없습니다.");
    } else {
        Serial.printf("%d개의 와이파이가 검색되었습니다:\n", n);
        for (int i = 0; i < n; ++i) {
            Serial.printf("%d: %s (신호강도: %d)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
    }
    */

    // core 0 유휴 감시 해제: 시끄러울 때 TLS 핸드셰이크+추론이 겹치면 5초 넘게 꽉 차서 감시가 재부팅을 걸기 때문
    esp_task_wdt_config_t wdt_cfg = { CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000, 0, true };
    esp_task_wdt_reconfigure(&wdt_cfg);

    audio_init();
    battery_init();
    tls_alloc_use_psram();
    ai_ws_start();
    motor_init();
    led_init();
    backend_ws_start();
    ota_start();
}

#if DEBUG_METRICS_ENABLED
// 내부 RAM 여유 확인(10초마다)
static void heap_log_update() {
    static uint32_t last_ms = 0;
    uint32_t now = millis();
    if (now - last_ms < 10000) return;
    last_ms = now;
    Serial.printf("[heap] 내부 %u (최저 %u, 최대블록 %u) / PSRAM %u\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}
#endif

void loop() {
#if DEBUG_METRICS_ENABLED
    heap_log_update();
#endif
    motor_update();
    battery_update();
    led_periodic_update();
    detector_process();
}
