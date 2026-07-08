/*
 * tiny-gpu 算子: 向量加法 / 点积 / ReLU / MatMul
 *
 * 这些是 "GPU kernel" 的等价物——在虚拟设备上运行的计算。
 * 目前由于 QEMU 设备只支持简单操作码，部分复杂算子分多次调用。
 */

#include "tgpu_runtime.h"
#include <math.h>
#include <string.h>

/* ── SGEMM: 朴素矩阵乘法 (C = A * B) ───────────── */

#define MATMUL_TILE  16

int tgpuSgemm(
    const float *A, const float *B, float *C,
    int M, int N, int K)
{
    /* 朴素实现: C = A * B
     *   A: M × K
     *   B: K × N
     *   C: M × N
     */
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < K; k++) {
                sum += A[i * K + k] * B[k * N + j];
            }
            C[i * N + j] = sum;
        }
    }
    return 0;
}

/* ── SGEMM tiled (分块, 模拟 GPU 上 tiling 策略) ── */

int tgpuSgemmTiled(
    const float *A, const float *B, float *C,
    int M, int N, int K)
{
    memset(C, 0, M * N * sizeof(float));

    for (int ii = 0; ii < M; ii += MATMUL_TILE) {
        for (int jj = 0; jj < N; jj += MATMUL_TILE) {
            for (int kk = 0; kk < K; kk += MATMUL_TILE) {

                /* 每个 tile 做一次小矩阵乘法 */
                int i_max = (ii + MATMUL_TILE < M) ? ii + MATMUL_TILE : M;
                int j_max = (jj + MATMUL_TILE < N) ? jj + MATMUL_TILE : N;
                int k_max = (kk + MATMUL_TILE < K) ? kk + MATMUL_TILE : K;

                for (int i = ii; i < i_max; i++) {
                    for (int j = jj; j < j_max; j++) {
                        float sum = C[i * N + j];
                        for (int k = kk; k < k_max; k++) {
                            sum += A[i * K + k] * B[k * N + j];
                        }
                        C[i * N + j] = sum;
                    }
                }
            }
        }
    }
    return 0;
}

/* ── ReLU ──────────────────────────────────────── */

int tgpuReLU(float *data, int count)
{
    for (int i = 0; i < count; i++) {
        if (data[i] < 0.0f)
            data[i] = 0.0f;
    }
    return 0;
}

/* ── Softmax ───────────────────────────────────── */

int tgpuSoftmax(float *data, int count)
{
    /* 找最大值 (numerical stability) */
    float max_val = data[0];
    for (int i = 1; i < count; i++) {
        if (data[i] > max_val)
            max_val = data[i];
    }

    /* exp(x_i - max) */
    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        data[i] = expf(data[i] - max_val);
        sum += data[i];
    }

    /* 归一化 */
    for (int i = 0; i < count; i++) {
        data[i] /= sum;
    }

    return 0;
}

/* ── 向量加法 (通过 QEMU 设备) ─────────────────── */

int tgpuOpVectorAdd(float *c, const float *a, const float *b,
                    uint32_t count)
{
    return tgpuVectorAddFloat(c, a, b, count);
}

/* ── 向量点积 ──────────────────────────────────── */

float tgpuOpDot(const float *a, const float *b, int count)
{
    float result = 0.0f;
    for (int i = 0; i < count; i++) {
        result += a[i] * b[i];
    }
    return result;
}

/* ── Conv2D (朴素, 微型实现) ────────────────────── */

int tgpuOpConv2D(
    const float *input, const float *kernel,
    float *output,
    int H, int W,      /* 输入尺寸 */
    int KH, int KW,    /* 卷积核尺寸 */
    int C_in, int C_out /* 通道数 */
)
{
    int OH = H - KH + 1;  /* 输出高度 */
    int OW = W - KW + 1;  /* 输出宽度 */

    memset(output, 0, C_out * OH * OW * sizeof(float));

    for (int oc = 0; oc < C_out; oc++) {
        for (int ic = 0; ic < C_in; ic++) {
            for (int oh = 0; oh < OH; oh++) {
                for (int ow = 0; ow < OW; ow++) {

                    float sum = 0.0f;
                    for (int kh = 0; kh < KH; kh++) {
                        for (int kw = 0; kw < KW; kw++) {
                            int in_idx = ((ic * H + oh + kh) * W + ow + kw);
                            int k_idx  = ((oc * C_in + ic) * KH + kh) * KW + kw;
                            sum += input[in_idx] * kernel[k_idx];
                        }
                    }
                    output[(oc * OH + oh) * OW + ow] += sum;
                }
            }
        }
    }
    return 0;
}
