#pragma once

#if defined(BOARD_ESP32_STD)
  #define I2S_SCK  14
  #define I2S_WS   15
  #define I2S0_SD  32   // mic_l (L ch), mic_r (R ch)
  #define I2S1_SD  16   // mic_b (L ch)
#elif defined(BOARD_ESP32_S3)
  #define I2S_SCK  15
  #define I2S_WS   14
  #define I2S0_SD  17
  #define I2S1_SD  18
#else
  #error "대상 보드가 정의되지 않았습니다. platformio.ini의 build_flags를 확인하세요."
#endif

constexpr int SAMPLE_RATE            = 16000;
constexpr int BLOCK_SIZE             = 256;
constexpr int SEND_INTERVAL_SAMPLES  = SAMPLE_RATE / 2;
constexpr int BYTES_PER_FRAME        = sizeof(int32_t) * 2;

constexpr int TRIGGER_THRESHOLD      = 30;
constexpr int SILENCE_THRESHOLD      = 20;
constexpr int SILENCE_COUNT_MAX      = 1;

constexpr int   MAX_TDOA_SAMPLES = 16;
constexpr float TDOA_THRESHOLD   = 1.5f;
constexpr int   VOTE_BUF_SIZE    = SAMPLE_RATE / BLOCK_SIZE + 1;
