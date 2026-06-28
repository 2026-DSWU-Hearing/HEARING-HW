#pragma once
#include <stdint.h>
#include "direction.h"
#include "config.h"

void      transport_init();
void      transport_poll();
int16_t*  transport_get_pcm_buf();
// num_samples: 전송할 샘플 수. 1초치는 SAMPLE_RATE.
void      transport_send(Direction dir, int num_samples);
