#pragma once

// setup()에서 1회 호출. 백엔드 웹소켓 연결을 Core 0 태스크로 분리해 Core 1 오디오 캡처와 독립적으로 처리.
void backend_ws_start();
