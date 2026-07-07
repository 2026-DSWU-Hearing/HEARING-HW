#include "direction.h"
#include "config.h"
#include <string.h>
#include <math.h>
#include <Arduino.h>

static Direction vote_buf[VOTE_BUF_SIZE];
static int       vote_idx = 0;

void direction_reset() {
    for (int i = 0; i < VOTE_BUF_SIZE; i++) vote_buf[i] = Direction::UNKNOWN;
    vote_idx = 0;
}

// 서브샘플 보간 포함 cross-correlation peak 위치 반환 (float)
// 반환값 d > 0: a가 b보다 앞서 도달 / d < 0: b가 a보다 앞서 도달
static float cross_corr_peak(const int16_t* a, const int16_t* b) {
    constexpr int N = 2 * MAX_TDOA_SAMPLES + 1;
    float corr[N];

    for (int d = -MAX_TDOA_SAMPLES; d <= MAX_TDOA_SAMPLES; d++) {
        float s = 0;
        for (int i = 0; i < BLOCK_SIZE; i++) {
            int j = i + d;
            if (j >= 0 && j < BLOCK_SIZE) s += (float)a[i] * (float)b[j];
        }
        corr[d + MAX_TDOA_SAMPLES] = s;
    }

    // 정수 피크 탐색
    int best_idx = 0;
    for (int k = 1; k < N; k++) {
        if (corr[k] > corr[best_idx]) best_idx = k;
    }

    // 경계에서는 보간 불가 → 정수값 반환
    if (best_idx == 0 || best_idx == N - 1)
        return (float)(best_idx - MAX_TDOA_SAMPLES);

    // 포물선 보간
    float y0 = corr[best_idx - 1];
    float y1 = corr[best_idx];
    float y2 = corr[best_idx + 1];
    float denom = y0 - 2.0f * y1 + y2;
    float offset = (denom != 0.0f) ? 0.5f * (y0 - y2) / denom : 0.0f;

    return (float)(best_idx - MAX_TDOA_SAMPLES) + offset;
}

void direction_update(const int16_t* l, const int16_t* r, const int16_t* b) {
    long ea_l = 0, ea_r = 0;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        ea_l += abs(l[i]);
        ea_r += abs(r[i]);
    }
    if (max(ea_l, ea_r) / BLOCK_SIZE < TRIGGER_THRESHOLD) return;

    float tdoa_lr = cross_corr_peak(l, r);
    float tdoa_lb = cross_corr_peak(l, b);
    float tdoa_rb = cross_corr_peak(r, b);

    Serial.printf("tdoa_lr: %.2f, tdoa_lb: %.2f, tdoa_rb: %.2f\n", tdoa_lr, tdoa_lb, tdoa_rb);

    Direction vote;
    if (tdoa_lb < -TDOA_THRESHOLD && tdoa_rb < -TDOA_THRESHOLD) {
        vote = Direction::BACK;
    } else if (tdoa_lr > TDOA_THRESHOLD) {
        vote = Direction::LEFT;
    } else if (tdoa_lr < -TDOA_THRESHOLD) {
        vote = Direction::RIGHT;
    } else if (tdoa_lb > TDOA_THRESHOLD && tdoa_rb > TDOA_THRESHOLD) {
        vote = Direction::FRONT;
    } else {
        return;  // 애매한 블록은 투표 건너뜀
    }

    vote_buf[vote_idx] = vote;
    vote_idx = (vote_idx + 1) % VOTE_BUF_SIZE;
}

Direction direction_get() {
    int count[4] = {0};
    for (int i = 0; i < VOTE_BUF_SIZE; i++) {
        uint8_t v = (uint8_t)vote_buf[i];
        if (v < 4) count[v]++;
    }
    int best = -1, best_count = 0;
    for (int d = 0; d < 4; d++) {
        if (count[d] > best_count) { best_count = count[d]; best = d; }
    }
    return best >= 0 ? (Direction)best : Direction::UNKNOWN;
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
