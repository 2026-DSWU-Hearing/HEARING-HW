#pragma once
#include <stdint.h>

enum class Direction : uint8_t {
    FRONT   = 0,
    BACK    = 1,
    LEFT    = 2,
    RIGHT   = 3,
    UNKNOWN = 4
};

void        direction_reset();
void        direction_update(const int16_t* l, const int16_t* r, const int16_t* b, long energy_l, long energy_r, long energy_b, int frames);
Direction   direction_get();
const char* direction_to_str(Direction d);
