#pragma once
#include <stdint.h>
#include "config.h"

void audio_init();

// I2S에서 한 블록 읽기. ring buffer 갱신 + 마이크별 에너지 누적.
// energy_* 는 누적(reset은 호출부 책임). frames 수 반환.
int  audio_read_block(long* energy_l, long* energy_r, long* energy_b);

// ring buffer → out_buf (SAMPLE_RATE개 int16) 평탄화
void audio_flatten(int16_t* out_buf);

// 가장 최근 블록의 L/R/B 샘플 복사 (TDOA 방향 추정용, direction_update에 전달)
void audio_get_last_block(int16_t* l_out, int16_t* r_out, int16_t* b_out);

// 지금부터 ms 동안 ring buffer 기록을 중단(모터 진동 노이즈 유입 방지).
// backend_ws 태스크(Core 0)에서 호출, audio_read_block(Core 1)에서 매 블록 확인 — 단일 uint32_t라 뮤텍스 불필요.
void audio_mute(uint32_t ms);
bool audio_is_muted();
