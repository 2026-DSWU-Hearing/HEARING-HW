#include "ota.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_ota_ops.h>
#include "secrets.h"
#include "net_log.h"

static const uint32_t OTA_VERIFY_TIMEOUT_MS = 3 * 60 * 1000;  // WiFi 연결 실패 시 이전 코드로 되돌림
static const uint32_t OTA_VERIFY_STABLE_MS  = 60 * 1000;

// 부팅 시 자동 확정 끄기
extern "C" bool verifyRollbackLater() { return true; }

static bool pending_verify() {
    esp_ota_img_states_t state;
    return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
           state == ESP_OTA_IMG_PENDING_VERIFY;
}

static void setup_callbacks() {
    ArduinoOTA.setPassword(ota_password);
    ArduinoOTA.setMdnsEnabled(false);  // 내부 RAM 절약
    ArduinoOTA.onStart([]() { Serial.println("[OTA] 업데이트 시작"); });
    ArduinoOTA.onEnd([]()   { Serial.println("[OTA] 업데이트 완료, 재부팅"); });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        static unsigned int last_pct = 100;
        unsigned int pct = total ? done * 100 / total : 0;
        if (pct / 10 != last_pct / 10) {
            last_pct = pct;
            Serial.printf("[OTA] %u%%\n", pct);
        }
    });
    ArduinoOTA.onError([](ota_error_t err) { Serial.printf("[OTA] 실패 (코드 %d)\n", (int)err); });
}

static void ota_task(void* param) {
    bool verify = pending_verify();
    Serial.printf("[OTA] 빌드 %s %s, 파티션 %s%s\n", __DATE__, __TIME__,
                  esp_ota_get_running_partition()->label, verify ? ", 새 펌웨어 검증 대기" : "");
    setup_callbacks();

    uint32_t boot_ms       = millis();
    uint32_t wifi_since_ms = 0;
    bool     wifi_was      = false;
    bool     started       = false;

    for (;;) {
        bool wifi = WiFi.status() == WL_CONNECTED;
        if (wifi && !started) {
            ArduinoOTA.begin();
            started = true;
            Serial.printf("[OTA] 업로드 대기: %s:3232\n", WiFi.localIP().toString().c_str());
        }
        if (started) ArduinoOTA.handle();

        if (verify) {
            uint32_t now = millis();
            if (wifi && !wifi_was) wifi_since_ms = now;
            wifi_was = wifi;
            if (wifi && now - wifi_since_ms >= OTA_VERIFY_STABLE_MS) {
                esp_ota_mark_app_valid_cancel_rollback();
                verify = false;
                Serial.println("[OTA] 새 펌웨어 검증 통과");
            } else if (now - boot_ms >= OTA_VERIFY_TIMEOUT_MS) {
                Serial.println("[OTA] WiFi 연결 실패, 이전 펌웨어로 되돌림");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ota_start() {
    xTaskCreatePinnedToCore(ota_task, "ota", 8192, NULL, 1, NULL, 0);
}
