// [!] 안내 사항
// 1. 이 파일을 'secrets.h'로 이름을 변경한 뒤 본인의 정보를 입력하여 사용하세요.
// 2. 혹은 main.cpp 내의 #include "secrets.h" 를 #include "secrets_example.h" 로 
// 변경하여 사용해도 무방합니다.
#ifndef SECRETS_H
#define SECRETS_H
#include <stdint.h>

static const char* ssid = "your_ssid";
static const char* password = "your_password";

// AI서버 오디오 전송 주소. wss://면 TLS(ca_cert.h) 사용, 로컬 테스트는 "ws://<PC IP>:8765/ws/neckband"
static const char* ai_server_ws_url = "wss://your_server_domain/ws/neckband";

// 백엔드 기기 채널 주소(?token=&mac= 는 코드가 붙임). 로컬 테스트는 "ws://<PC IP>:8000/ws/devices"
static const char* backend_ws_url = "wss://your_server_domain/ws/devices";
static const char* backend_device_token = "your_device_token";

// WiFi 로그(net_log, 텔넷 23번) 접속 암호. NET_LOG_ENABLED=1일 때만 사용.
static const char* net_log_password = "your_net_log_password";

#endif
