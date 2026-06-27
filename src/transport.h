#pragma once
#include <stdint.h>
#include "direction.h"
#include "config.h"

void      transport_init();
void      transport_poll();
int16_t*  transport_get_pcm_buf();
void      transport_send(Direction dir, int sample_count);
