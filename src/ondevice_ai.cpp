#include "ondevice_ai.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <math.h>

#include "features.h"
#include "model_data.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

namespace {

// 모델 작업 메모리(텐서 아레나). 추정 55~80KB라 여유 있게 96KB.
// 실제 사용량은 init 로그에 찍히니 그 값을 보고 줄여도 됨.
constexpr size_t ARENA_SIZE       = 96 * 1024;
constexpr int    MODEL_INPUT_SIZE = FEAT_FRAMES * FEAT_MELS;  // 98 x 40

uint8_t*                  g_arena       = nullptr;
tflite::MicroInterpreter* g_interpreter = nullptr;
TfLiteTensor*             g_input       = nullptr;
TfLiteTensor*             g_output      = nullptr;
bool                      g_ready       = false;

float g_logmel[MODEL_INPUT_SIZE];  // 특징추출 결과 (15KB라 정적 할당)

tflite::MicroMutableOpResolver<6> g_resolver;  // 모델이 쓰는 연산 6가지

}  // namespace

bool ondevice_ai_init() {
    if (g_ready) return true;

    const tflite::Model* model = tflite::GetModel(g_emergency_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.printf("[온디바이스AI] 모델 버전 불일치: 모델=%d, 런타임=%d\n",
                      (int)model->version(), (int)TFLITE_SCHEMA_VERSION);
        return false;
    }

    // 내부 RAM이 더 빠르지만 다 써버리면 WiFi가 쓸 메모리가 모자랄 수 있음.
    // 잡고도 INTERNAL_MARGIN 이상 남을 때만 내부 RAM, 아니면 PSRAM에 할당.
    constexpr size_t INTERNAL_MARGIN = 80 * 1024;
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    Serial.printf("[온디바이스AI] 내부 RAM 여유 %u, PSRAM 여유 %u 바이트\n",
                  (unsigned)free_internal, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    const char* where = "내부 RAM";
    if (free_internal >= ARENA_SIZE + INTERNAL_MARGIN) {
        g_arena = (uint8_t*)heap_caps_aligned_alloc(16, ARENA_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (g_arena == nullptr) {
        where = "PSRAM";
        g_arena = (uint8_t*)heap_caps_aligned_alloc(16, ARENA_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (g_arena == nullptr) {
        Serial.println("[온디바이스AI] 작업 메모리 할당 실패");
        return false;
    }

    g_resolver.AddConv2D();
    g_resolver.AddDepthwiseConv2D();
    g_resolver.AddFullyConnected();
    g_resolver.AddLogistic();
    g_resolver.AddMaxPool2D();
    g_resolver.AddMean();

    static tflite::MicroInterpreter interpreter(model, g_resolver, g_arena, ARENA_SIZE);
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        // 흔한 원인: 위 목록에 없는 연산 / 아레나 부족
        Serial.println("[온디바이스AI] AllocateTensors 실패 (연산 미등록 또는 메모리 부족)");
        return false;
    }

    g_interpreter = &interpreter;
    g_input  = interpreter.input(0);
    g_output = interpreter.output(0);

    // 입출력이 예상한 형태(int8, 입력 98x40)인지 확인
    if (g_input->type != kTfLiteInt8 || g_output->type != kTfLiteInt8 ||
        g_input->bytes != (size_t)MODEL_INPUT_SIZE) {
        Serial.printf("[온디바이스AI] 모델 입출력 형식 불일치 (입력 %d바이트)\n", (int)g_input->bytes);
        return false;
    }

    Serial.printf("[온디바이스AI] 준비 완료: 작업 메모리 %u/%u 바이트 사용 (%s), 입력 scale=%.5f zp=%d\n",
                  (unsigned)interpreter.arena_used_bytes(), (unsigned)ARENA_SIZE, where,
                  g_input->params.scale, (int)g_input->params.zero_point);
    g_ready = true;
    return true;
}

float ondevice_ai_infer(const int16_t* pcm, uint32_t* feature_us, uint32_t* infer_us) {
    if (!g_ready) return -1.0f;

    int64_t t0 = esp_timer_get_time();
    features_compute(pcm, g_logmel);
    int64_t t1 = esp_timer_get_time();

    // float 특징 -> int8 입력 (scale/zero_point는 모델이 알려주는 값 사용)
    const float scale = g_input->params.scale;
    const int   zp    = g_input->params.zero_point;
    int8_t*     dst   = g_input->data.int8;
    for (int i = 0; i < MODEL_INPUT_SIZE; i++) {
        int q = (int)lrintf(g_logmel[i] / scale) + zp;
        if (q < -128) q = -128;
        if (q > 127)  q = 127;
        dst[i] = (int8_t)q;
    }

    if (g_interpreter->Invoke() != kTfLiteOk) {
        Serial.println("[온디바이스AI] 추론 실패");
        return -1.0f;
    }
    int64_t t2 = esp_timer_get_time();

    // int8 출력 -> 확률 (scale=1/256, zero_point=-128)
    float prob = ((int)g_output->data.int8[0] - (int)g_output->params.zero_point) * g_output->params.scale;
    if (prob < 0.0f) prob = 0.0f;
    if (prob > 1.0f) prob = 1.0f;

    if (feature_us) *feature_us = (uint32_t)(t1 - t0);
    if (infer_us)   *infer_us   = (uint32_t)(t2 - t1);
    return prob;
}
