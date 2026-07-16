#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "audio.h"
#include "direction.h"
#include "ai_server_ws.h"
#include "backend_ws.h"
#include "motor.h"

enum class State { IDLE, GATHERING, STREAMING };


void setup() {
    Serial.begin(115200);

    Serial.println("\n주변 와이파이 검색 중...");
    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.println("검색된 와이파이가 없습니다.");
    } else {
        Serial.printf("%d개의 와이파이가 검색되었습니다:\n", n);
        for (int i = 0; i < n; ++i) {
            Serial.printf("%d: %s (신호강도: %d)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
    }

    audio_init();
    ai_ws_init();
    motor_init();
    backend_ws_start();
}

void loop() {
    ai_ws_poll();
    motor_update();

    static State state          = State::IDLE;
    static int   sample_counter = 0;
    static int   silence_counter = 0;
    static long  energy_acc = 0;

    long block_el = 0, block_er = 0, block_eb = 0;
    int  frames = audio_read_block(&block_el, &block_er, &block_eb);
    long block_max = max(block_el, max(block_er, block_eb));

    // 진동 직후 무음 구간: I2S는 계속 드레인하되 트리거/방향투표/전송은 전부 스킵.
    if (audio_is_muted()) return;

    if (state == State::IDLE) {
        if (frames > 0 && block_max / frames > TRIGGER_THRESHOLD) {
            state          = State::GATHERING;
            sample_counter = frames;
            energy_acc     = block_max;
            direction_reset();
            int16_t bl[BLOCK_SIZE], br[BLOCK_SIZE], bb[BLOCK_SIZE];
            audio_get_last_block(bl, br, bb);
            direction_update(bl, br, bb, block_el, block_er, block_eb, frames);
        }
        return;
    }

    {
        int16_t bl[BLOCK_SIZE], br[BLOCK_SIZE], bb[BLOCK_SIZE];
        audio_get_last_block(bl, br, bb);
        direction_update(bl, br, bb, block_el, block_er, block_eb, frames);
    }

    energy_acc += block_max;
    sample_counter += frames;

    if (sample_counter < SEND_INTERVAL_SAMPLES) return;

    Direction dir = direction_get();
    Serial.printf("방향: %s\n", direction_to_str(dir));
    direction_reset();

    audio_flatten(ai_ws_get_pcm_buf());
    ai_ws_send(dir, SAMPLE_RATE);

    long avg_volume = energy_acc / sample_counter;
    Serial.printf("volume: %ld\n", avg_volume);

    if (state == State::GATHERING) state = State::STREAMING;

    sample_counter = 0;
    energy_acc     = 0;

    if (avg_volume < SILENCE_THRESHOLD) {
        if (++silence_counter >= SILENCE_COUNT_MAX) {
            state          = State::IDLE;
            silence_counter = 0;
        }
    } else {
        silence_counter = 0;
    }
}
