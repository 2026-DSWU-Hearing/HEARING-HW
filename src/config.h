#pragma once
#include <stdint.h>

#if defined(BOARD_ESP32_STD)
  #define I2S_SCK  14
  #define I2S_WS   15
  #define I2S0_SD  32   // mic_l (L ch), mic_r (R ch)
  #define I2S1_SD  16   // mic_b (L ch)

  // 모터/배터리/LED 미배선. 값 자체는 미사용.
  #define MOTOR_PIN_LEFT   4
  #define MOTOR_PIN_RIGHT  5
  #define MOTOR_PIN_BACK   6
  #define BATTERY_ADC_PIN  2
  #define LED_RED_PIN      7
  #define LED_GREEN_PIN    8
#elif defined(BOARD_ESP32_S3)
  #define I2S_SCK  15
  #define I2S_WS   14
  #define I2S0_SD  17
  #define I2S1_SD  18

  #define MOTOR_PIN_LEFT   4
  #define MOTOR_PIN_RIGHT  5
  #define MOTOR_PIN_BACK   6

  #define BATTERY_ADC_PIN  2  // 배터리 전압 분배(100K/100K, 1/2 분배) 측정 핀
  
  #define LED_RED_PIN      7  // 배터리 잔량 표시 LED(적색)
  #define LED_GREEN_PIN    8  // 배터리 잔량 표시 LED(녹색)
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
constexpr int   VOTE_BUF_SIZE    = SAMPLE_RATE / BLOCK_SIZE + 1;

// 방향 판정 (GCC-PHAT, 시간차 단위: 샘플)
constexpr float ONSET_RATIO          = 1.5f;  // 직전 블록 대비 증가율(울림 제외)
constexpr float CLOSURE_TOL          = 2.0f;  // 세 쌍 검산 허용 오차
constexpr float LR_MAX_LAG           = 8.0f;  // 왼-오 17cm
constexpr float FB_MAX_LAG           = 11.7f; // 앞-뒤 25cm
constexpr float MIN_DIR_MAG          = 0.2f;  // 각도 판정 최소 크기
constexpr float FRONT_HALF_ANGLE_DEG = 30.0f;
constexpr float BACK_HALF_ANGLE_DEG  = 60.0f;

// 검산 실패 시 보조 판정
constexpr float LR_ONLY_MIN          = 2.0f;
constexpr float LR_ONLY_MAX          = 10.0f; // 초과 시 lb-rb로 재확인
constexpr float LR_PAIR_MIN          = 3.0f;
constexpr float BACK_LAG_MIN         = 2.0f;  // 뒤 판정 기준(lb, rb 모두 음수)

constexpr uint32_t AUDIO_MUTE_AFTER_VIBRATE_MS = 1000; // 모터 진동 시작부터 노이즈 방지용 무음 구간
constexpr uint32_t VIBRATE_DURATION_MS = 500; // 진동 지속시간 0.5초
constexpr uint32_t VIBRATE_COOLDOWN_MS = 3000; // 온디바이스 진동 쿨다운 3초

// 온디바이스 AI 사용 (문제 시 0으로 빌드)
#ifndef ONDEVICE_AI_ENABLED
#define ONDEVICE_AI_ENABLED 1
#endif

// WiFi 로그 (테스트용, net_log.h)
#ifndef NET_LOG_ENABLED
#define NET_LOG_ENABLED 0
#endif

// 측정 로그([heap], [전송측정]). netlog 환경에서 켜짐
#ifndef DEBUG_METRICS_ENABLED
#define DEBUG_METRICS_ENABLED 0
#endif

constexpr float    BATTERY_DIVIDER_RATIO      = 2.0f;  // 100K/100K 분배
constexpr float    BATTERY_FULL_VOLTAGE       = 4.2f;  // 100%
constexpr float    BATTERY_EMPTY_VOLTAGE      = 3.3f;  // 0%(보호회로 컷오프보다 여유)
constexpr uint8_t  BATTERY_LOW_THRESHOLD_PCT  = 20;    // LED 빨강 기준(이하)
constexpr uint32_t BATTERY_CHECK_INTERVAL_MS  = 5000;
constexpr int      BATTERY_ADC_SAMPLES        = 32;    // 1회 측정 평균 횟수
constexpr float    BATTERY_SMOOTHING_DOWN     = 0.05f; // 하강 반영 비율
constexpr float    BATTERY_SMOOTHING_UP       = 0.3f;  // 상승 반영 비율
constexpr uint8_t  BATTERY_CHARGE_JUMP_PCT    = 5;     // 충전 판단 상승폭

constexpr float MOTOR_RATED_VOLTAGE = 3.0f;       // 코인형 진동모터 정격전압
constexpr float MOTOR_MIN_VALID_BATTERY_V = 2.0f; // 전압 보정 하한(미만은 ADC 오류로 봄)
