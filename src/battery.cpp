#include "battery.h"
#include <Arduino.h>
#include "config.h"

void battery_init() {
    // analogReadMilliVolts()가 eFuse 보정을 알아서 처리. 별도 설정 불필요
}

float battery_get_voltage() {
    uint32_t mv = analogReadMilliVolts(BATTERY_ADC_PIN);
    return (mv / 1000.0f) * BATTERY_DIVIDER_RATIO;
}

uint8_t battery_get_percent() {
    float v = battery_get_voltage();
    float pct = (v - BATTERY_EMPTY_VOLTAGE) / (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return (uint8_t)pct;
}
