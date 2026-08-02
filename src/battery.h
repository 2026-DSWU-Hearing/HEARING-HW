#pragma once
#include <stdint.h>

void  battery_init();
float battery_get_voltage();
// 0~100
uint8_t battery_get_percent();
