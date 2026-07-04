#pragma once
#include <stdint.h>
#include "direction.h"
#include "config.h"

void      websocket_init();
void      websocket_poll();
int16_t*  websocket_get_pcm_buf();
// num_samples: 전송할 샘플 수. 1초치는 SAMPLE_RATE.
void      websocket_send(Direction dir, int num_samples);
