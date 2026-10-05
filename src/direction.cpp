#include "direction.h"
#include "config.h"
#include "features.h"
#include <math.h>
#include <Arduino.h>
#include "net_log.h"

constexpr int   FFT_N   = 512;
constexpr float RAD2DEG = 57.29578f;

static Direction vote_buf[VOTE_BUF_SIZE];
static int       vote_idx    = 0;
static Direction held_dir    = Direction::UNKNOWN; // 이번 소리에서 마지막으로 정한 방향
static float     prev_energy = 0.0f;

// GCC-PHAT 작업 버퍼 (core 1 전용)
constexpr int HALF = FFT_N / 2;
constexpr int LAGS = 2 * MAX_TDOA_SAMPLES + 1;
static float s_re[3][FFT_N];
static float s_im[3][FFT_N];
static float s_gr[3][HALF + 1];
static float s_gi[3][HALF + 1];
static float s_zero[HALF + 1];
static float s_xr[FFT_N];
static float s_xi[FFT_N];

static void clear_votes() {
    for (int i = 0; i < VOTE_BUF_SIZE; i++) vote_buf[i] = Direction::UNKNOWN;
    vote_idx = 0;
}

void direction_reset() {
    clear_votes();
    held_dir    = Direction::UNKNOWN;
    prev_energy = 0.0f;
}

void direction_next_window() {
    clear_votes();
}

static void spectrum(const int16_t* x, float* re, float* im) {
    for (int i = 0; i < BLOCK_SIZE; i++) { re[i] = x[i]; im[i] = 0.0f; }
    for (int i = BLOCK_SIZE; i < FFT_N; i++) { re[i] = 0.0f; im[i] = 0.0f; }
    fft512(re, im);
}

// conj(A)*B 를 크기^0.75로 나눔(특정 높이의 소리가 계산을 지배하지 않게). 대칭이라 절반만 계산
static void cross_spectrum(int a, int b, float* gr, float* gi) {
    gr[0] = 0.0f;
    gi[0] = 0.0f;
    for (int k = 1; k <= HALF; k++) {
        float r = s_re[a][k] * s_re[b][k] + s_im[a][k] * s_im[b][k];
        float i = s_re[a][k] * s_im[b][k] - s_im[a][k] * s_re[b][k];
        float mag = sqrtf(r * r + i * i) + 1e-9f;
        float inv = 1.0f / (sqrtf(mag) * sqrtf(sqrtf(mag)));
        gr[k] = r * inv;
        gi[k] = i * inv;
    }
}

// 두 상관함수를 역FFT 한 번으로 계산(g1 + j*g2). 결과는 corr1, corr2에 지연 -16~16 순서로
static void inverse_pair(const float* g1r, const float* g1i, const float* g2r, const float* g2i,
                         float* corr1, float* corr2) {
    s_xr[0] = 0.0f;
    s_xi[0] = 0.0f;
    for (int k = 1; k < HALF; k++) {
        s_xr[k]         = g1r[k] - g2i[k];  // 역FFT = conj(FFT(conj(x)))
        s_xi[k]         = -(g1i[k] + g2r[k]);
        s_xr[FFT_N - k] = g1r[k] + g2i[k];
        s_xi[FFT_N - k] = g1i[k] - g2r[k];
    }
    s_xr[HALF] = g1r[HALF];
    s_xi[HALF] = -g2r[HALF];
    fft512(s_xr, s_xi);

    for (int d = -MAX_TDOA_SAMPLES; d <= MAX_TDOA_SAMPLES; d++) {
        int idx = (d + FFT_N) % FFT_N;
        corr1[d + MAX_TDOA_SAMPLES] = s_xr[idx];
        corr2[d + MAX_TDOA_SAMPLES] = -s_xi[idx];
    }
}

// 피크 위치(서브샘플). d>0: 앞 마이크가 먼저 도달. 범위 끝이면 NAN(계산 실패)
static float peak(const float* corr) {
    int best = 0;
    for (int k = 1; k < LAGS; k++) {
        if (corr[k] > corr[best]) best = k;
    }
    if (best == 0 || best == LAGS - 1) return NAN;

    // 포물선 보간
    float y0 = corr[best - 1], y1 = corr[best], y2 = corr[best + 1];
    float denom  = y0 - 2.0f * y1 + y2;
    float offset = (denom != 0.0f) ? 0.5f * (y0 - y2) / denom : 0.0f;
    return (float)(best - MAX_TDOA_SAMPLES) + offset;
}

static Direction decide(float lr, float lb, float rb, const char** why) {
    // 뒤 쌍 둘 다 뒤 마이크에 먼저 도착했으면 뒤 (검산과 무관)
    if (!isnan(lb) && !isnan(rb) && lb <= -BACK_LAG_MIN && rb <= -BACK_LAG_MIN) {
        *why = "뒤쌍";
        return Direction::BACK;
    }

    bool lr_ok = !isnan(lr);
    bool full  = lr_ok && !isnan(lb) && !isnan(rb) && fabsf(lb - (lr + rb)) <= CLOSURE_TOL;

    if (full) {
        float x = lr / LR_MAX_LAG;
        float y = (lb + rb) * 0.5f / FB_MAX_LAG;
        if (x * x + y * y < MIN_DIR_MAG * MIN_DIR_MAG) { *why = "약함"; return Direction::UNKNOWN; }
        float ang = atan2f(x, y) * RAD2DEG; // 0 정면, +90 오른쪽, ±180 뒤
        *why = "";
        if (fabsf(ang) <= FRONT_HALF_ANGLE_DEG)         return Direction::FRONT;
        if (fabsf(ang) >= 180.0f - BACK_HALF_ANGLE_DEG) return Direction::BACK;
        return ang > 0 ? Direction::RIGHT : Direction::LEFT;
    }

    // 검산 실패: 뒤 쌍은 안 믿고 좌우만 판정
    if (lr_ok && fabsf(lr) >= LR_ONLY_MIN) {
        *why = "좌우만";
        return lr > 0 ? Direction::RIGHT : Direction::LEFT;
    }
    *why = "무효";
    return Direction::UNKNOWN;
}

void direction_update(const int16_t* l, const int16_t* r, const int16_t* b, long energy_l, long energy_r, long energy_b, int frames) {
    // skip되는 블록도 슬롯 자체는 항상 흘려보내야 vote_buf가 실제 경과 시간과 맞음
    Direction vote = Direction::UNKNOWN;

    float energy = frames > 0 ? (float)max(energy_l, max(energy_r, energy_b)) / frames : 0.0f;
    bool onset = energy >= TRIGGER_THRESHOLD && energy >= prev_energy * ONSET_RATIO;
    prev_energy = energy;

    if (onset) {
        uint32_t t0 = micros();
        spectrum(l, s_re[0], s_im[0]);
        spectrum(r, s_re[1], s_im[1]);
        spectrum(b, s_re[2], s_im[2]);
        cross_spectrum(0, 1, s_gr[0], s_gi[0]);
        cross_spectrum(0, 2, s_gr[1], s_gi[1]);
        cross_spectrum(1, 2, s_gr[2], s_gi[2]);

        float c_lr[LAGS], c_lb[LAGS], c_rb[LAGS], c_unused[LAGS];
        inverse_pair(s_gr[0], s_gi[0], s_gr[1], s_gi[1], c_lr, c_lb);
        inverse_pair(s_gr[2], s_gi[2], s_zero, s_zero, c_rb, c_unused);
        float tdoa_lr = peak(c_lr);
        float tdoa_lb = peak(c_lb);
        float tdoa_rb = peak(c_rb);

        const char* why = "";
        vote = decide(tdoa_lr, tdoa_lb, tdoa_rb, &why);
        Serial.printf("tdoa_lr: %.2f, tdoa_lb: %.2f, tdoa_rb: %.2f -> %s %s (%lu us)\n",
                      tdoa_lr, tdoa_lb, tdoa_rb, direction_to_str(vote), why,
                      (unsigned long)(micros() - t0));
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
    if (best >= 0) held_dir = (Direction)best;
    return held_dir; // 이번 구간에 표가 없으면 직전 방향 유지
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
