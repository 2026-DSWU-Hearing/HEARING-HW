// [!] 안내 사항
// 1. 이 파일을 'secrets.h'로 이름을 변경한 뒤 본인의 정보를 입력하여 사용하세요.
// 2. 혹은 main.cpp 내의 #include "secrets.h" 를 #include "secrets_example.h" 로 
// 변경하여 사용해도 무방합니다.
#ifndef SECRETS_H
#define SECRETS_H
#include <stdint.h>

const char* ssid = "your_ssid";
const char* password = "your_password";
const char* websockets_server_host = "your_server_ip";
const uint16_t websockets_server_port = 8765;

#endif