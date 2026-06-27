#pragma once
#include <stdint.h>
#include "config.h"

void audio_init();

// I2S에서 한 블록 읽기. ring buffer 갱신 + 마이크별 에너지 누적.
// energy_* 는 누적(reset은 호출부 책임). frames 수 반환.
int  audio_read_block(long* energy_l, long* energy_r, long* energy_b);

// ring buffer → out_buf (SAMPLE_RATE개 int16) 평탄화
void audio_flatten(int16_t* out_buf);
