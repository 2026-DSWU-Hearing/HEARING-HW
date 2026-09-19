"""mel 가중치 표(src/mel_table.h) 생성.

학습 때 쓴 특징추출(features.py)의 tf.signal.linear_to_mel_weight_matrix 결과를 그대로 뽑아 C++ 헤더로 만듦.
(행렬 257x40 중 0이 아닌 부분만 저장)

사용법 (tensorflow가 있는 파이썬으로):
    python tools/gen_mel_table.py > src/mel_table.h

파라미터는 학습 설정(config.py)의 값과 동일해야 함.
"""
import numpy as np
import tensorflow as tf

SAMPLE_RATE = 16000
FFT_LENGTH = 512
NUM_MEL_BINS = 40
LOWER_FREQUENCY = 125.0
UPPER_FREQUENCY = 7500.0
BINS = FFT_LENGTH // 2 + 1  # 257

m = tf.signal.linear_to_mel_weight_matrix(
    NUM_MEL_BINS, BINS, SAMPLE_RATE, LOWER_FREQUENCY, UPPER_FREQUENCY
).numpy().astype(np.float32)  # (257, 40)

starts, counts, weights = [], [], []
for k in range(NUM_MEL_BINS):
    nz = np.nonzero(m[:, k])[0]
    lo, hi = int(nz.min()), int(nz.max())
    starts.append(lo)
    counts.append(hi - lo + 1)          # 범위 안에 0이 섞여 있어도 그대로 포함
    weights.extend(m[lo:hi + 1, k].tolist())


def fmt_ints(v):
    return ", ".join(str(x) for x in v)


def fmt_floats(v, per_line=6):
    lines = []
    for i in range(0, len(v), per_line):
        lines.append("    " + ", ".join(f"{x:.9g}f" for x in v[i:i + per_line]) + ",")
    return "\n".join(lines)


print("// 자동 생성 파일, 직접 수정 금지 (tools/gen_mel_table.py)")
print("// tf.signal.linear_to_mel_weight_matrix(40, 257, 16000, 125.0, 7500.0)의 0이 아닌 부분")
print("#pragma once")
print()
print(f"constexpr int MEL_BANDS = {NUM_MEL_BINS};")
print(f"constexpr int MEL_TABLE_SIZE = {len(weights)};")
print()
print("// 밴드 k: 파워 스펙트럼의 MEL_START[k]번 빈부터 MEL_COUNT[k]개에 가중치를 곱해 더함")
print(f"static const int MEL_START[{NUM_MEL_BINS}] = {{ {fmt_ints(starts)} }};")
print(f"static const int MEL_COUNT[{NUM_MEL_BINS}] = {{ {fmt_ints(counts)} }};")
print()
print(f"static const float MEL_WEIGHTS[{len(weights)}] = {{")
print(fmt_floats(weights))
print("};")
