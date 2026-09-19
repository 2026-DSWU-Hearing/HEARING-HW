#pragma once
#include <stdint.h>

// 온디바이스 위험음 분류 (TFLite Micro, int8 모델 v8)
// 1초 PCM -> log-mel(features.cpp) -> 모델 -> 긴급일 확률(0~1)

// 이 확률 이상이면 긴급으로 판정 (높을수록 확실할 때만 진동).
// 모델 검증 결과(threshold_v8.json) 기준 0.5 이상부터 정밀도는 0.93~0.97에서 정체, 재현율만 하락.
// 놓친 소리는 AI서버가 다시 잡으므로 정밀도 쪽인 0.6 선택 (정밀도 0.95, 재현율 0.67).
// 최종 test 데이터에선 성능이 더 낮았으니(0.175에서 정밀도 0.72) 실기기에서 조정할 것.
constexpr float ONDEVICE_AI_THRESHOLD = 0.6f;

// 모델 로딩과 작업 메모리 확보. 추론을 돌릴 태스크 안에서 1번 호출. 실패 시 false.
bool ondevice_ai_init();

// pcm: int16 x 16000. 반환: 긴급일 확률(0~1), 실패 시 -1.
// feature_us / infer_us: 특징추출 / 모델 추론 소요 시간(us), 필요 없으면 nullptr.
// init 성공 후 같은 태스크에서만 호출할 것 (내부 작업 버퍼 공유).
float ondevice_ai_infer(const int16_t* pcm, uint32_t* feature_us, uint32_t* infer_us);
