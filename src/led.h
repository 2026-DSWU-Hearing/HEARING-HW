#pragma once
#include <stdint.h>

void led_init();
// percent: 0~100. BATTERY_LOW_THRESHOLD_PCT 이하이면 빨강, 초과면 초록.
void led_update(uint8_t percent);
// loop()에서 매 회 호출: BATTERY_CHECK_INTERVAL_MS 간격으로 배터리 잔량을 LED에 반영.
void led_periodic_update();
