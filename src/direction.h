#pragma once
#include <stdint.h>

enum class Direction : uint8_t {
    FRONT   = 0,
    BACK    = 1,
    LEFT    = 2,
    RIGHT   = 3,
    UNKNOWN = 4
};

void        direction_reset();       // 새 소리 시작
void        direction_next_window(); // 0.5초 전송 구간마다
void        direction_update(const int16_t* l, const int16_t* r, const int16_t* b, long energy_l, long energy_r, long energy_b, int frames);
Direction   direction_get();
const char* direction_to_str(Direction d);
