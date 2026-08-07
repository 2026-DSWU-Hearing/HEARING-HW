#pragma once
#include <Arduino.h>
#include <ArduinoWebsockets.h>

// 여러 태스크에서 불러도 안전 — 내부 타이머가 재시도 간격을 막아줌.
void wifi_ensure_connected();

// url_builder는 재시도 시점에만 호출됨(매 루프마다 문자열 생성 방지).
bool ws_ensure_connected(websockets::WebsocketsClient& client, String (*url_builder)(),
                          const char* label, uint32_t interval_ms, uint32_t& last_attempt_ms);
