#pragma once
#include <stdint.h>
#include "direction.h"

void motor_init();

// strength: 0~100 (haptic_strength 그대로)
void motor_vibrate(Direction dir, uint8_t strength);

// loop()에서 매 회 호출: VIBRATE_DURATION_MS 경과 시 자동으로 진동 정지
void motor_update();
