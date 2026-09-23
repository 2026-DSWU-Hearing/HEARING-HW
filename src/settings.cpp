#include "settings.h"

// 백엔드 설정 수신 전 기본값
static volatile bool    s_emergency_alert_enabled = true;
static volatile bool    s_do_not_disturb          = false;
static volatile uint8_t s_haptic_strength         = 70;  // 오프라인 기본 세기

bool    settings_emergency_alert_enabled() { return s_emergency_alert_enabled; }
bool    settings_do_not_disturb()          { return s_do_not_disturb; }
uint8_t settings_haptic_strength()         { return s_haptic_strength; }

void settings_set(bool emergency_alert_enabled, bool do_not_disturb, uint8_t haptic_strength) {
    s_emergency_alert_enabled = emergency_alert_enabled;
    s_do_not_disturb          = do_not_disturb;
    s_haptic_strength         = haptic_strength > 100 ? 100 : haptic_strength;
}
