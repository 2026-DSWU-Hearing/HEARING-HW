#pragma once

// loop()에서 매 회 호출: I2S 블록 읽기 + 트리거 판정 + 방향 투표 + AI서버 전송까지 처리.
void detector_process();
