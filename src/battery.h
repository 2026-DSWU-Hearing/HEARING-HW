#pragma once
#include <stdint.h>

void  battery_init();
void  battery_update();
float battery_get_voltage();
uint8_t battery_get_percent(); // 0~100, 충전 없이는 오르지 않음
