#include "led.h"
#include <Arduino.h>
#include "config.h"
#include "battery.h"

void led_init() {
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_RED_PIN, OUTPUT);
}

void led_update(uint8_t percent) {
    bool low = percent < BATTERY_LOW_THRESHOLD_PCT;
    digitalWrite(LED_GREEN_PIN, low ? LOW : HIGH);
    digitalWrite(LED_RED_PIN,   low ? HIGH : LOW);
}

void led_periodic_update() {
    static uint32_t last_check_ms = 0;
    uint32_t now = millis();
    if (now - last_check_ms >= BATTERY_CHECK_INTERVAL_MS) {
        last_check_ms = now;
        led_update(battery_get_percent());
    }
}
