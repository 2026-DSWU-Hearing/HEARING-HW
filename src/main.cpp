#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <driver/i2s_std.h>
#include "secrets.h"

using namespace websockets;
WebsocketsClient client;

// I2S pin map (varies by target board)
#if defined(BOARD_ESP32_STD)
  #define I2S_SCK  14
  #define I2S_WS   15
  #define I2S0_SD  32
  #define I2S1_SD  16
#elif defined(BOARD_ESP32_S3)
  #define I2S_SCK  15
  #define I2S_WS   14
  #define I2S0_SD  17
  #define I2S1_SD  18
#else
  #error "대상 보드가 정의되지 않았습니다. platformio.ini의 build_flags를 확인하세요."
#endif

// Audio & trigger settings
constexpr int SAMPLE_RATE           = 16000;
constexpr int BLOCK_SIZE            = 256;
constexpr int SEND_INTERVAL_SAMPLES = SAMPLE_RATE / 2;
constexpr int BYTES_PER_FRAME       = sizeof(int32_t) * 2;

constexpr int TRIGGER_THRESHOLD = 100;
constexpr int SILENCE_THRESHOLD = 20;
constexpr int SILENCE_COUNT_MAX = 1;

// FSM state definition
enum State { IDLE, GATHERING_FUTURE, STREAMING };

// I2S channel handles
i2s_chan_handle_t rx_handle_0 = NULL;
i2s_chan_handle_t rx_handle_1 = NULL;

// buffer
int32_t samples_i2s0[BLOCK_SIZE * 2];
int32_t samples_i2s1[BLOCK_SIZE * 2];
int16_t ring_buffer[SAMPLE_RATE];
int16_t audio_buffer[SAMPLE_RATE];

int write_index = 0;

// I2S initialization
void i2s_init() {
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

  i2s_std_config_t std_cfg0 = {
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
  i2s_channel_init_std_mode(rx_handle_0, &std_cfg0);

  i2s_std_config_t std_cfg1 = std_cfg0;
  std_cfg1.gpio_cfg.din = (gpio_num_t)I2S1_SD;
  i2s_channel_init_std_mode(rx_handle_1, &std_cfg1);

  i2s_channel_enable(rx_handle_0);
  i2s_channel_enable(rx_handle_1);
}

// Flatten ring buffer, transmit audio, and return average volume
long calculate_and_send_audio() {
  int read_idx = write_index;
  long total_energy = 0;

  for (int i = 0; i < SAMPLE_RATE; i++) {
    int16_t sample = ring_buffer[read_idx];
    audio_buffer[i] = sample;
    total_energy += abs(sample);
    read_idx = (read_idx + 1) % SAMPLE_RATE;
  }

  client.sendBinary((const char*)audio_buffer, sizeof(audio_buffer));
  return total_energy / SAMPLE_RATE;
}

void setup() {
  Serial.begin(115200);
  i2s_init();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected!");

  client.onMessage([](WebsocketsMessage message) {
    Serial.println(message.data());
  });

  while (!client.connect(websockets_server_host, websockets_server_port, websockets_server_path)) {
    Serial.println("WebSocket 연결 실패. 재시도 중...");
    delay(3000);
  }
  Serial.println("WebSocket Connected!");
}

void loop() {
  // Handle incoming server messages
  if (client.available()) client.poll();

  // Read raw audio from I2S
  size_t r0 = 0, r1 = 0;
  i2s_channel_read(rx_handle_0, samples_i2s0, sizeof(samples_i2s0), &r0, portMAX_DELAY);
  i2s_channel_read(rx_handle_1, samples_i2s1, sizeof(samples_i2s1), &r1, portMAX_DELAY);

  int frames_read = r0 / BYTES_PER_FRAME;

  static State current_state  = IDLE;
  static int   sample_counter  = 0;
  static int   silence_counter = 0;

  for (int i = 0; i < frames_read; i++) {
    // 32 bit -> 16 bit
    int16_t left = samples_i2s0[i * 2] >> 16;

    // ring buffer
    ring_buffer[write_index] = left;
    write_index = (write_index + 1) % SAMPLE_RATE;

    // FSM
    if (current_state == IDLE) {
      if (abs(left) > TRIGGER_THRESHOLD) {
        current_state = GATHERING_FUTURE;
        sample_counter = 0;
      }
    }
    else {
      sample_counter++;

      // Send audio when 0.5s of samples accumulated
      if (sample_counter >= SEND_INTERVAL_SAMPLES) {
        sample_counter = 0;

        if (current_state == GATHERING_FUTURE) {
          current_state = STREAMING;
        } 

        long avg_volume = calculate_and_send_audio();
        Serial.printf("volume: %ld\n", avg_volume);

        if (avg_volume < SILENCE_THRESHOLD) {
          silence_counter++;
          if (silence_counter >= SILENCE_COUNT_MAX) {
            current_state  = IDLE;
            silence_counter = 0;
          }
        } 
        else {
          silence_counter = 0;
        }
      }
    }
  }
}
