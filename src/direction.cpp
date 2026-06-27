#include "direction.h"
#include "config.h"

Direction calculate_direction(long energy_l, long energy_r, long energy_b) {
    if (energy_l == 0 && energy_r == 0 && energy_b == 0) return Direction::UNKNOWN;

    float fl = (float)energy_l;
    float fr = (float)energy_r;
    float fb = (float)energy_b;

    bool l_dom = (fl > fr * DIRECTION_DOMINANT_RATIO) && (fl > fb * DIRECTION_DOMINANT_RATIO);
    bool r_dom = (fr > fl * DIRECTION_DOMINANT_RATIO) && (fr > fb * DIRECTION_DOMINANT_RATIO);
    bool b_dom = (fb > fl * DIRECTION_DOMINANT_RATIO) && (fb > fr * DIRECTION_DOMINANT_RATIO);

    if (l_dom) return Direction::LEFT;
    if (r_dom) return Direction::RIGHT;
    if (b_dom) return Direction::BACK;
    if (fl > fb && fr > fb) return Direction::FRONT;

    return Direction::UNKNOWN;
}

const char* direction_to_str(Direction d) {
    switch (d) {
        case Direction::FRONT:   return "FRONT";
        case Direction::BACK:    return "BACK";
        case Direction::LEFT:    return "LEFT";
        case Direction::RIGHT:   return "RIGHT";
        default:                 return "UNKNOWN";
    }
}
