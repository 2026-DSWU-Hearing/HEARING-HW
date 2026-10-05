#pragma once
// 테스트용: 시리얼 로그를 WiFi(텔넷 23번)로도 출력. 보기: pio device monitor --port socket://<보드IP>:23
// 이 헤더를 포함한 파일의 Serial을 NetLog로 바꿔치기함.
#include <Arduino.h>
#include "config.h"

class NetLogPrint : public Print {
public:
    void begin(unsigned long baud);
    using Print::write;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t* buf, size_t size) override;
};

extern NetLogPrint NetLog;

#if NET_LOG_ENABLED && !defined(NET_LOG_IMPL)
#undef Serial
#define Serial NetLog
#endif
