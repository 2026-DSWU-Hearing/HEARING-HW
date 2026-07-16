#pragma once
#include <stdint.h>
#include "direction.h"

void motor_init();

// strength: 0~100 (haptic_strength 그대로)
void motor_vibrate(Direction dir, uint8_t strength);
