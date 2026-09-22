#define NET_LOG_IMPL  // 이 파일 안의 Serial은 진짜 UART 시리얼
#include "net_log.h"

#include <WiFi.h>
#include <esp_heap_caps.h>
#include <string.h>
#include "secrets.h"

NetLogPrint NetLog;

#if NET_LOG_ENABLED

namespace {

constexpr uint16_t NET_LOG_PORT     = 23;
constexpr size_t   NET_LOG_BUF_SIZE = 16 * 1024;  // PSRAM
constexpr size_t   SEND_CHUNK       = 512;

uint8_t*     g_buf  = nullptr;
size_t       g_head = 0;  // 다음에 쓸 위치
size_t       g_len  = 0;  // 쌓인 바이트 수
portMUX_TYPE g_mux  = portMUX_INITIALIZER_UNLOCKED;

// 가득 차면 오래된 로그부터 버림
void buf_push(const uint8_t* data, size_t n) {
    if (g_buf == nullptr || n == 0) return;
    if (n > NET_LOG_BUF_SIZE) {
        data += n - NET_LOG_BUF_SIZE;
        n = NET_LOG_BUF_SIZE;
    }
    portENTER_CRITICAL(&g_mux);
    size_t first = NET_LOG_BUF_SIZE - g_head;
    if (first > n) first = n;
    memcpy(g_buf + g_head, data, first);
    memcpy(g_buf, data + first, n - first);
    g_head = (g_head + n) % NET_LOG_BUF_SIZE;
    g_len  = (g_len + n > NET_LOG_BUF_SIZE) ? NET_LOG_BUF_SIZE : g_len + n;
    portEXIT_CRITICAL(&g_mux);
}

size_t buf_pop(uint8_t* out, size_t max_n) {
    portENTER_CRITICAL(&g_mux);
    size_t n     = (g_len < max_n) ? g_len : max_n;
    size_t tail  = (g_head + NET_LOG_BUF_SIZE - g_len) % NET_LOG_BUF_SIZE;
    size_t first = NET_LOG_BUF_SIZE - tail;
    if (first > n) first = n;
    memcpy(out, g_buf + tail, first);
    memcpy(out + first, g_buf, n - first);
    g_len -= n;
    portEXIT_CRITICAL(&g_mux);
    return n;
}

// 접속 시 암호(secrets.h) 한 줄을 확인해야 로그 스트리밍 시작
constexpr size_t LINE_BUF_SIZE = 64;

void try_authenticate(WiFiClient& client, bool& authed, char* line, size_t& line_len) {
    while (client.available()) {
        char c = (char)client.read();
        if (c == '\n' || c == '\r') {
            if (line_len == 0) continue;
            line[line_len] = '\0';
            line_len = 0;
            if (strcmp(line, net_log_password) == 0) {
                authed = true;
                client.println("OK");
            } else {
                client.println("denied");
                client.stop();
            }
            return;
        }
        if (line_len < LINE_BUF_SIZE - 1) line[line_len++] = c;
    }
}

// 쌓인 로그를 WiFi로 전송. 접속은 1개만
void net_log_task(void* param) {
    vTaskDelay(pdMS_TO_TICKS(1000));  // WiFi.begin() 이후에 시작

    WiFiServer server(NET_LOG_PORT);
    WiFiClient client;
    bool       listening = false;
    bool       authed    = false;
    char       line[LINE_BUF_SIZE];
    size_t     line_len  = 0;
    IPAddress  last_ip;
    uint8_t    chunk[SEND_CHUNK];

    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            if (client) client.stop();
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (!listening) {
            server.begin();
            server.setNoDelay(true);
            listening = true;
        }
        if (WiFi.localIP() != last_ip) {
            last_ip = WiFi.localIP();
            NetLog.printf("[WiFi로그] 접속 대기: socket://%s:%u\n", last_ip.toString().c_str(), NET_LOG_PORT);
        }

        if (server.hasClient()) {
            if (client) client.stop();
            client   = server.accept();
            client.setNoDelay(true);
            authed   = false;
            line_len = 0;
            client.println("password:");
        }

        if (client && client.connected()) {
            if (!authed) {
                try_authenticate(client, authed, line, line_len);
            } else {
                while (client.available()) client.read();  // 단방향 로그, 입력 무시
                size_t n;
                while ((n = buf_pop(chunk, sizeof(chunk))) > 0) {
                    if (client.write(chunk, n) != n) {
                        client.stop();
                        break;
                    }
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

}  // namespace

void NetLogPrint::begin(unsigned long baud) {
    Serial.begin(baud);
    if (g_buf != nullptr) return;

    g_buf = (uint8_t*)heap_caps_malloc(NET_LOG_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (g_buf == nullptr) {
        Serial.println("[WiFi로그] 버퍼 할당 실패, WiFi 로그 없이 동작");
        return;
    }
    xTaskCreatePinnedToCore(net_log_task, "net_log", 4096, NULL, 1, NULL, 0);
}

size_t NetLogPrint::write(uint8_t c) {
    return write(&c, 1);
}

size_t NetLogPrint::write(const uint8_t* buf, size_t size) {
    size_t n = Serial.write(buf, size);
    buf_push(buf, size);
    return n;
}

#else

void NetLogPrint::begin(unsigned long baud) { Serial.begin(baud); }
size_t NetLogPrint::write(uint8_t c) { return Serial.write(c); }
size_t NetLogPrint::write(const uint8_t* buf, size_t size) { return Serial.write(buf, size); }

#endif
