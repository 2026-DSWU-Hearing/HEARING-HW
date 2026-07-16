#include "ai_server_ws.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "secrets.h"

using namespace websockets;

static WebsocketsClient ai_ws_client;

// [1바이트 방향][3바이트 패딩][PCM int16 데이터]
static uint8_t send_buf[4 + SAMPLE_RATE * sizeof(int16_t)];

void ai_ws_init() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi Connected!");

    ai_ws_client.onMessage([](WebsocketsMessage msg) {
        Serial.println(msg.data());
    });

    String url = String("ws://") + websockets_server_host + ":" + websockets_server_port + "/ws";
    while (!ai_ws_client.connect(url)) {
        Serial.println("AI서버 WebSocket 연결 실패. 재시도 중...");
        delay(3000);
    }
    Serial.println("AI서버 WebSocket Connected!");
}

void ai_ws_poll() {
    if (ai_ws_client.available()) ai_ws_client.poll();
}

int16_t* ai_ws_get_pcm_buf() {
    return (int16_t*)(send_buf + 4);
}

void ai_ws_send(Direction dir, int num_samples) {
    send_buf[0] = static_cast<uint8_t>(dir);
    send_buf[1] = 0;
    send_buf[2] = 0;
    send_buf[3] = 0;
    ai_ws_client.sendBinary((const char*)send_buf, 4 + num_samples * sizeof(int16_t));
}
