#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "audio.h"
#include "direction.h"
#include "transport.h"

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
    transport_init();
}

void loop() {
    transport_poll();

    static State state          = State::IDLE;
    static int   sample_counter = 0;
    static int   silence_counter = 0;
    static long  energy_l = 0, energy_r = 0, energy_b = 0;

    long block_el = 0, block_er = 0, block_eb = 0;
    int  frames = audio_read_block(&block_el, &block_er, &block_eb);

    if (state == State::IDLE) {
        // TODO: 시리얼 플로터용 임시 코드 - 확인 후 삭제
        if (frames > 0)
            Serial.printf(">L:%ld|R:%ld|B:%ld\n",
                          block_el/frames, block_er/frames, block_eb/frames);
        long block_max = max(block_el, max(block_er, block_eb));
        if (frames > 0 && block_max / frames > TRIGGER_THRESHOLD) {
            state          = State::GATHERING;
            sample_counter = frames;
            energy_l = block_el;
            energy_r = block_er;
            energy_b = block_eb;
        }
        return;
    }

    energy_l += block_el;
    energy_r += block_er;
    energy_b += block_eb;
    sample_counter += frames;

    if (sample_counter < SEND_INTERVAL_SAMPLES) return;

    // 0.5초 블록 완성 → 방향 계산 후 전송
    Direction dir = calculate_direction(energy_l, energy_r, energy_b);
    Serial.printf("방향: %s  (L:%ld R:%ld B:%ld)\n",
                  direction_to_str(dir), energy_l, energy_r, energy_b);

    audio_flatten(transport_get_pcm_buf());
    transport_send(dir, SAMPLE_RATE);

    long avg_volume = energy_l / sample_counter;
    Serial.printf("volume: %ld\n", avg_volume);

    if (state == State::GATHERING) state = State::STREAMING;

    sample_counter = 0;
    energy_l = energy_r = energy_b = 0;

    if (avg_volume < SILENCE_THRESHOLD) {
        if (++silence_counter >= SILENCE_COUNT_MAX) {
            state          = State::IDLE;
            silence_counter = 0;
        }
    } else {
        silence_counter = 0;
    }
}
