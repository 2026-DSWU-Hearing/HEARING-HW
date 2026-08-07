#include "motor.h"
#include <Arduino.h>
#include "audio.h"
#include "battery.h"
#include "config.h"

constexpr int PWM_FREQ = 200;
constexpr int PWM_RES  = 8;              // ledcWrite duty: 0~255
constexpr int PWM_MAX_DUTY = (1 << PWM_RES) - 1;

static volatile uint32_t vibrate_until_ms = 0;

void motor_init() {
    ledcAttach(MOTOR_PIN_LEFT,  PWM_FREQ, PWM_RES);
    ledcAttach(MOTOR_PIN_RIGHT, PWM_FREQ, PWM_RES);
    ledcAttach(MOTOR_PIN_BACK,  PWM_FREQ, PWM_RES);
}

// 모터는 배터리 원 전압(3.0~4.2V)을 그대로 받으므로, 전압 대비 듀티를 보정해 실효 전압을 strength% of MOTOR_RATED_VOLTAGE로 고정.
static void write_duty(int pin, uint8_t strength) {
    float battery_v = battery_get_voltage();
    // 비정상적으로 낮으면(ADC 미배선 등) 보정 포기 — 안 그러면 나눗셈 결과가 비정상적으로 커져 항상 최대 duty로 튐
    if (battery_v < MOTOR_MIN_VALID_BATTERY_V) {
        battery_v = MOTOR_RATED_VOLTAGE;
    }
    float target_v  = (strength / 100.0f) * MOTOR_RATED_VOLTAGE;
    float duty_frac  = target_v / battery_v;
    duty_frac = constrain(duty_frac, 0.0f, 1.0f);
    ledcWrite(pin, (int)(duty_frac * PWM_MAX_DUTY));
}

static void all_off() {
    ledcWrite(MOTOR_PIN_LEFT, 0);
    ledcWrite(MOTOR_PIN_RIGHT, 0);
    ledcWrite(MOTOR_PIN_BACK, 0);
}

void motor_vibrate(Direction dir, uint8_t strength) {
    all_off();
    switch (dir) {
        case Direction::LEFT:
            write_duty(MOTOR_PIN_LEFT, strength);
            audio_mute(AUDIO_MUTE_AFTER_VIBRATE_MS);
            vibrate_until_ms = millis() + VIBRATE_DURATION_MS;
            break;
        case Direction::RIGHT:
            write_duty(MOTOR_PIN_RIGHT, strength);
            audio_mute(AUDIO_MUTE_AFTER_VIBRATE_MS);
            vibrate_until_ms = millis() + VIBRATE_DURATION_MS;
            break;
        case Direction::BACK:
            write_duty(MOTOR_PIN_BACK, strength);
            audio_mute(AUDIO_MUTE_AFTER_VIBRATE_MS);
            vibrate_until_ms = millis() + VIBRATE_DURATION_MS;
            break;
        case Direction::FRONT:
            // Front는 진동 없음
            break;
        case Direction::UNKNOWN:
        default:
            // TODO: 방향 불확실 시 진동 정책 추후 논의 예정. 우선은 진동 없음(웹앱 알림만).
            break;
    }
}

// loop()에서 폴링: 진동 시작 후 VIBRATE_DURATION_MS가 지나면 자동으로 정지 (한 번 탭 형태)
void motor_update() {
    if (vibrate_until_ms == 0) return;
    if ((int32_t)(millis() - vibrate_until_ms) >= 0) {
        all_off();
        vibrate_until_ms = 0;
    }
}
