#include "websocket.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "secrets.h"

using namespace websockets;

static WebsocketsClient ws_client;

// [1바이트 방향][3바이트 패딩][PCM int16 데이터]
static uint8_t send_buf[4 + SAMPLE_RATE * sizeof(int16_t)];

void websocket_init() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi Connected!");

    ws_client.onMessage([](WebsocketsMessage msg) {
        Serial.println(msg.data());
    });

    while (!ws_client.connect(websockets_server_host, websockets_server_port, websockets_server_path)) {
        Serial.println("WebSocket 연결 실패. 재시도 중...");
        delay(3000);
    }
    Serial.println("WebSocket Connected!");
}

void websocket_poll() {
    if (ws_client.available()) ws_client.poll();
}

int16_t* websocket_get_pcm_buf() {
    return (int16_t*)(send_buf + 4);
}

void websocket_send(Direction dir, int num_samples) {
    send_buf[0] = static_cast<uint8_t>(dir);
    send_buf[1] = 0;
    send_buf[2] = 0;
    send_buf[3] = 0;
    ws_client.sendBinary((const char*)send_buf, 4 + num_samples * sizeof(int16_t));
}
