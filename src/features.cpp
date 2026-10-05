#include "features.h"

#include <math.h>

#include "mel_table.h"

// 계산 순서 (모델 학습 때 쓴 특징추출과 동일)
//   프레임 자르기 -> Hann 윈도우 -> FFT -> 파워 -> mel 필터 -> log -> 전체 정규화

namespace {

constexpr int    FRAME_LEN = 400;  // 25ms
constexpr int    FRAME_HOP = 160;  // 10ms
constexpr int    FFT_N     = 512;
constexpr int    SPEC_BINS = FFT_N / 2 + 1;  // 257
constexpr double PI_D      = 3.14159265358979323846;

// 작업 버퍼 (스택 대신 정적 할당)
float g_hann[FRAME_LEN];
float g_cos[FFT_N / 2];  // FFT 회전 인자 cos
float g_sin[FFT_N / 2];  // FFT 회전 인자 -sin
float g_re[FFT_N];
float g_im[FFT_N];
float g_power[SPEC_BINS];
bool  g_ready = false;

void init_tables() {
  // hann_window 기본값이 periodic 방식이라 분모는 FRAME_LEN
  for (int n = 0; n < FRAME_LEN; n++) {
    g_hann[n] = (float)(0.5 - 0.5 * cos(2.0 * PI_D * n / FRAME_LEN));
  }
  for (int k = 0; k < FFT_N / 2; k++) {
    g_cos[k] = (float)cos(2.0 * PI_D * k / FFT_N);
    g_sin[k] = (float)(-sin(2.0 * PI_D * k / FFT_N));
  }
  g_ready = true;
}

// 제자리 radix-2 FFT (512점)
void fft512(float* re, float* im) {
  // 비트 뒤집기 순서로 재배열
  int j = 0;
  for (int i = 0; i < FFT_N - 1; i++) {
    if (i < j) {
      float tr = re[i]; re[i] = re[j]; re[j] = tr;
      float ti = im[i]; im[i] = im[j]; im[j] = ti;
    }
    int m = FFT_N >> 1;
    while (m >= 1 && j >= m) { j -= m; m >>= 1; }
    j += m;
  }
  // 길이 2, 4, ... 512 순으로 합쳐 나감
  for (int len = 2; len <= FFT_N; len <<= 1) {
    int half = len >> 1;
    int step = FFT_N / len;
    for (int start = 0; start < FFT_N; start += len) {
      for (int k = 0; k < half; k++) {
        float wr = g_cos[k * step];
        float wi = g_sin[k * step];
        int a = start + k;
        int b = a + half;
        float tr = re[b] * wr - im[b] * wi;
        float ti = re[b] * wi + im[b] * wr;
        re[b] = re[a] - tr;
        im[b] = im[a] - ti;
        re[a] += tr;
        im[a] += ti;
      }
    }
  }
}

}  // namespace

void features_compute(const int16_t* pcm, float* out) {
  if (!g_ready) init_tables();

  for (int f = 0; f < FEAT_FRAMES; f++) {
    const int16_t* frame = pcm + f * FRAME_HOP;

    // 윈도우 곱하기(int16 -> -1~1), 512점이 되도록 뒤는 0으로 채움
    for (int i = 0; i < FRAME_LEN; i++) {
      g_re[i] = ((float)frame[i] / 32768.0f) * g_hann[i];
    }
    for (int i = FRAME_LEN; i < FFT_N; i++) g_re[i] = 0.0f;
    for (int i = 0; i < FFT_N; i++) g_im[i] = 0.0f;

    fft512(g_re, g_im);

    for (int k = 0; k < SPEC_BINS; k++) {
      g_power[k] = g_re[k] * g_re[k] + g_im[k] * g_im[k];
    }

    // mel 필터 + 로그
    const float* w = MEL_WEIGHTS;
    for (int m = 0; m < FEAT_MELS; m++) {
      const float* p = g_power + MEL_START[m];
      float sum = 0.0f;
      for (int i = 0; i < MEL_COUNT[m]; i++) sum += p[i] * w[i];
      w += MEL_COUNT[m];
      out[f * FEAT_MELS + m] = logf(sum + 1e-6f);
    }
  }

  // 전체(98x40)의 평균/표준편차로 정규화. 합계는 오차를 줄이려고 double로 계산.
  const int total = FEAT_FRAMES * FEAT_MELS;
  double mean = 0.0;
  for (int i = 0; i < total; i++) mean += out[i];
  mean /= total;

  double var = 0.0;
  for (int i = 0; i < total; i++) {
    double d = out[i] - mean;
    var += d * d;
  }
  double stddev = sqrt(var / total);

  // 원소별 계산은 float로 (ESP32-S3는 double이 소프트웨어 연산이라 느림). 나눗셈은 1번만 함.
  const float mean_f  = (float)mean;
  const float inv_std = (float)(1.0 / (stddev + 1e-6));
  for (int i = 0; i < total; i++) {
    out[i] = (out[i] - mean_f) * inv_std;
  }
}
