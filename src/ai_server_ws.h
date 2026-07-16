#pragma once
#include <stdint.h>
#include "direction.h"
#include "config.h"

void      ai_ws_init();
void      ai_ws_poll();
int16_t*  ai_ws_get_pcm_buf();
// num_samples: 전송할 샘플 수. 1초치는 SAMPLE_RATE.
void      ai_ws_send(Direction dir, int num_samples);
