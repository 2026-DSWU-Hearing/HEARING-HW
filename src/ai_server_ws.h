#pragma once
#include <stdint.h>
#include "direction.h"
#include "config.h"

// setup()에서 1회 호출. WiFi/AI서버 웹소켓 연결을 Core 0 태스크로 분리해 Core 1 오디오 캡처와 독립적으로 처리.
void      ai_ws_start();
// 전송용 빈 슬롯이 없으면 nullptr 반환 — 이때는 audio_flatten/ai_ws_send를 호출하지 말 것.
int16_t*  ai_ws_get_pcm_buf();
// num_samples: 전송할 샘플 수. 1초치는 SAMPLE_RATE. ai_ws_get_pcm_buf()가 유효한 포인터를 반환했을 때만 호출.
void      ai_ws_send(Direction dir, int num_samples);
