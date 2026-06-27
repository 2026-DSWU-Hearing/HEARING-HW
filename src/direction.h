#pragma once
#include <stdint.h>

enum class Direction : uint8_t {
    FRONT   = 0,
    BACK    = 1,
    LEFT    = 2,
    RIGHT   = 3,
    UNKNOWN = 4
};

Direction   calculate_direction(long energy_l, long energy_r, long energy_b);
const char* direction_to_str(Direction d);
