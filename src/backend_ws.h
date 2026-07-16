#pragma once

// setup()에서 1회 호출. 백엔드 /ws/devices 연결을 Core 0 태스크로 분리해서
// 오디오 캡처 루프(Core 1)와 완전히 독립적으로 재연결/진동수신을 처리한다.
void backend_ws_start();
