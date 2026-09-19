#pragma once
#include <stdint.h>

// 온디바이스 모델 입력(log-mel) 계산. 학습 때 쓴 waveform_to_log_mel과 같은 결과가 나오게 구현.
constexpr int FEAT_SAMPLES = 16000;  // 입력: 16kHz 1초
constexpr int FEAT_FRAMES  = 98;     // 출력 프레임 수
constexpr int FEAT_MELS    = 40;     // 출력 mel 밴드 수

// pcm: int16 x 16000 / out: float x (98 * 40), 행 우선 [프레임 * 40 + mel]
// 내부 작업 버퍼가 정적이라 한 태스크에서만 호출할 것.
void features_compute(const int16_t* pcm, float* out);
