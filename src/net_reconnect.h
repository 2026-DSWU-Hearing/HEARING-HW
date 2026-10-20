#pragma once
#include <Arduino.h>
#include <ArduinoWebsockets.h>

// 여러 태스크에서 불러도 안전 — 내부 타이머가 재시도 간격을 막아줌. 반환값: 현재 WiFi 연결 여부.
// 처음 연결되면 NTP 시간 동기화를 시작함(wss 인증서 검증에 필요).
bool wifi_ensure_connected();

// TLS 큰 버퍼를 PSRAM에 할당(내부 RAM 부족 방지). 웹소켓 태스크 시작 전 1회 호출.
void tls_alloc_use_psram();

// url이 wss://면 Let's Encrypt 루트 CA를 등록. 태스크 시작 시 1회 호출.
void ws_setup_tls(websockets::WebsocketsClient& client, const char* url);

// url_builder는 재시도 시점에만 호출됨(매 루프마다 문자열 생성 방지). wss://는 시간 동기화 전엔 시도하지 않음.
bool ws_ensure_connected(websockets::WebsocketsClient& client, String (*url_builder)(),
                          const char* label, uint32_t interval_ms, uint32_t& last_attempt_ms);
