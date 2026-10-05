#include "battery.h"
#include <Arduino.h>
#include "config.h"
#include "motor.h"

// 측정은 battery_update()에서만, 나머지는 저장값만 읽음
static volatile float   s_voltage = 0.0f;
static volatile uint8_t s_percent = 0;

static float read_voltage() {
    uint32_t sum_mv = 0;
    for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
        sum_mv += analogReadMilliVolts(BATTERY_ADC_PIN);
    }
    return (sum_mv / (float)BATTERY_ADC_SAMPLES / 1000.0f) * BATTERY_DIVIDER_RATIO;
}

static uint8_t to_percent(float v) {
    float pct = (v - BATTERY_EMPTY_VOLTAGE) / (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE) * 100.0f;
    if (pct < 0.0f)   pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return (uint8_t)pct;
}

void battery_init() {
    s_voltage = read_voltage();
    s_percent = to_percent(s_voltage);
}

void battery_update() {
    static uint32_t last_check_ms = 0;
    uint32_t now = millis();
    if (now - last_check_ms < BATTERY_CHECK_INTERVAL_MS) return;
    if (motor_is_active()) return; // 진동 중엔 전압이 처짐
    last_check_ms = now;

    // 순간 전압 처짐 무시
    float v = read_voltage();
    float k = (v < s_voltage) ? BATTERY_SMOOTHING_DOWN : BATTERY_SMOOTHING_UP;
    s_voltage = s_voltage * (1.0f - k) + v * k;

    // 충전 시에만 표시값 상승
    uint8_t pct = to_percent(s_voltage);
    if (pct < s_percent || pct >= s_percent + BATTERY_CHARGE_JUMP_PCT) {
        s_percent = pct;
    }
}

float battery_get_voltage() {
    return s_voltage;
}

uint8_t battery_get_percent() {
    return s_percent;
}
