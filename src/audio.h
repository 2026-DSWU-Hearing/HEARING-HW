#pragma once
#include <stdint.h>
#include "config.h"

void audio_init();

// I2S 한 블록 읽기 + ring buffer 갱신 + 에너지 누적(energy_*, reset은 호출부 책임). 반환값: frames 수.
int  audio_read_block(long* energy_l, long* energy_r, long* energy_b);

// ring buffer → out_buf (SAMPLE_RATE개 int16) 평탄화
void audio_flatten(int16_t* out_buf);

// 가장 최근 블록의 L/R/B 샘플 복사 (TDOA 방향 추정용, direction_update에 전달)
void audio_get_last_block(int16_t* l_out, int16_t* r_out, int16_t* b_out);

// ms 동안 ring buffer 기록 중단(진동 노이즈 방지). Core 0에서 호출, Core 1이 매 블록 확인 — 단일 uint32_t라 뮤텍스 불필요.
void audio_mute(uint32_t ms);
bool audio_is_muted();
