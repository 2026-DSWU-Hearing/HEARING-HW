#pragma once
#include <stdint.h>
#include "direction.h"

void motor_init();

// strength: 0~100, 실제로 진동하면 true 반환
bool motor_vibrate(Direction dir, uint8_t strength, bool from_ondevice);

// loop()에서 매 회 호출: VIBRATE_DURATION_MS 경과 시 자동으로 진동 정지
void motor_update();
