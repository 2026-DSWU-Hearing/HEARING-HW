#pragma once
#include <stdint.h>

// 기기에 보관하는 사용자 설정. 지금은 기본값으로 동작하고,
// 백엔드가 설정을 내려주면 settings_set()으로 갱신 예정.

// "긴급 소리 알림 받기". 꺼져 있으면 온디바이스 추론 안 함.
bool    settings_emergency_alert_enabled();
// 방해금지. 켜져 있으면 온디바이스 추론과 로컬 진동 안 함.
bool    settings_do_not_disturb();
// 진동 세기 0~100 (백엔드 haptic_strength와 같은 단위)
uint8_t settings_haptic_strength();

void    settings_set(bool emergency_alert_enabled, bool do_not_disturb, uint8_t haptic_strength);
