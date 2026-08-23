#include <Arduino.h>
#include <WiFi.h>
#include "audio.h"
#include "ai_server_ws.h"
#include "backend_ws.h"
#include "motor.h"
#include "battery.h"
#include "led.h"
#include "detector.h"

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

    audio_init();
    ai_ws_start();
    motor_init();
    battery_init();
    led_init();
    backend_ws_start();
}

void loop() {
    motor_update();
    led_periodic_update();
    detector_process();
}
