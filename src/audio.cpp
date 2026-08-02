#include "audio.h"
#include <Arduino.h>
#include <driver/i2s_std.h>

static i2s_chan_handle_t rx_handle_0 = NULL;
static i2s_chan_handle_t rx_handle_1 = NULL;

static int32_t samples_i2s0[BLOCK_SIZE * 2];
static int32_t samples_i2s1[BLOCK_SIZE * 2];
static int16_t ring_buf[SAMPLE_RATE];
static int     write_idx = 0;

static int16_t block_l[BLOCK_SIZE];
static int16_t block_r[BLOCK_SIZE];
static int16_t block_b[BLOCK_SIZE];

static volatile uint32_t mute_until_ms = 0;

void audio_mute(uint32_t ms) {
    mute_until_ms = millis() + ms;
}

bool audio_is_muted() {
    return (int32_t)(mute_until_ms - millis()) > 0;
}

void audio_init() {
    i2s_chan_config_t chan_cfg0 = {
        .id            = I2S_NUM_0,
        .role          = I2S_ROLE_MASTER,
        .dma_desc_num  = 6,
        .dma_frame_num = BLOCK_SIZE,
        .auto_clear    = false
    };
    i2s_new_channel(&chan_cfg0, NULL, &rx_handle_0);

    i2s_chan_config_t chan_cfg1 = {
        .id            = I2S_NUM_1,
        .role          = I2S_ROLE_SLAVE,
        .dma_desc_num  = 6,
        .dma_frame_num = BLOCK_SIZE,
        .auto_clear    = false
    };
    i2s_new_channel(&chan_cfg1, NULL, &rx_handle_1);

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src        = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple  = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask      = I2S_STD_SLOT_BOTH,
            .ws_width       = I2S_DATA_BIT_WIDTH_32BIT,
            .ws_pol         = false,
            .bit_shift      = true
        },
        .gpio_cfg = {
            .mclk         = I2S_GPIO_UNUSED,
            .bclk         = (gpio_num_t)I2S_SCK,
            .ws           = (gpio_num_t)I2S_WS,
            .dout         = I2S_GPIO_UNUSED,
            .din          = (gpio_num_t)I2S0_SD,
            .invert_flags = { 0, 0, 0 }
        }
    };
    i2s_channel_init_std_mode(rx_handle_0, &std_cfg);

    i2s_std_config_t std_cfg1 = std_cfg;
    std_cfg1.gpio_cfg.din = (gpio_num_t)I2S1_SD;
    i2s_channel_init_std_mode(rx_handle_1, &std_cfg1);

    i2s_channel_enable(rx_handle_0);
    i2s_channel_enable(rx_handle_1);
}

int audio_read_block(long* energy_l, long* energy_r, long* energy_b) {
    size_t r0 = 0, r1 = 0;
    i2s_channel_read(rx_handle_0, samples_i2s0, sizeof(samples_i2s0), &r0, portMAX_DELAY);
    i2s_channel_read(rx_handle_1, samples_i2s1, sizeof(samples_i2s1), &r1, portMAX_DELAY);

    bool muted = audio_is_muted();
    int frames = (int)(min(r0, r1) / BYTES_PER_FRAME);
    for (int i = 0; i < frames; i++) {
        int16_t l = (int16_t)(samples_i2s0[i * 2]     >> 16);
        int16_t r = (int16_t)(samples_i2s0[i * 2 + 1] >> 16);
        int16_t b = (int16_t)(samples_i2s1[i * 2]     >> 16);

        // 무음 중엔 ring buffer 쓰기만 스킵(진동 노이즈 방지). I2S 읽기는 DMA 타이밍 유지를 위해 계속 수행.
        if (!muted) {
            ring_buf[write_idx] = l;
            write_idx = (write_idx + 1) % SAMPLE_RATE;
        }

        block_l[i] = l;
        block_r[i] = r;
        block_b[i] = b;

        *energy_l += abs(l);
        *energy_r += abs(r);
        *energy_b += abs(b);
    }

    // 프레임 부족분은 0으로 채워 이전 블록의 잔여 샘플이 안 남게 함.
    for (int i = frames; i < BLOCK_SIZE; i++) {
        block_l[i] = 0;
        block_r[i] = 0;
        block_b[i] = 0;
    }

    return frames;
}

void audio_flatten(int16_t* out_buf) {
    int idx = write_idx;
    for (int i = 0; i < SAMPLE_RATE; i++) {
        out_buf[i] = ring_buf[idx];
        idx = (idx + 1) % SAMPLE_RATE;
    }
}

void audio_get_last_block(int16_t* l_out, int16_t* r_out, int16_t* b_out) {
    memcpy(l_out, block_l, BLOCK_SIZE * sizeof(int16_t));
    memcpy(r_out, block_r, BLOCK_SIZE * sizeof(int16_t));
    memcpy(b_out, block_b, BLOCK_SIZE * sizeof(int16_t));
}
