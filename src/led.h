#pragma once
#include <stdint.h>

void led_init();
// percent: 0~100. BATTERY_LOW_THRESHOLD_PCT 미만이면 빨강, 이상이면 초록.
void led_update(uint8_t percent);
// loop()에서 매 회 호출: BATTERY_CHECK_INTERVAL_MS 간격으로 배터리 잔량을 읽어 LED에 반영.
void led_periodic_update();
