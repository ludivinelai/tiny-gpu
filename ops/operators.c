/*
 * tiny-gpu 算子实现
 */

#include "operators.h"
#include <math.h>
#include <string.h>

#define MATMUL_TILE  16

/* ── SGEMM ─────────────────────────────────────── */

int tgpuKernelSgemm(
    const float *A, const float *B, float *C,
    int M, int N, int K)
{
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

int tgpuKernelSgemmTiled(
    const float *A, const float *B, float *C,
    int M, int N, int K)
{
    memset(C, 0, M * N * sizeof(float));

    for (int ii = 0; ii < M; ii += MATMUL_TILE) {
        for (int jj = 0; jj < N; jj += MATMUL_TILE) {
            for (int kk = 0; kk < K; kk += MATMUL_TILE) {

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

/* ── 激活函数 ───────────────────────────────────── */

int tgpuKernelReLU(float *data, int count)
{
    for (int i = 0; i < count; i++) {
        if (data[i] < 0.0f)
            data[i] = 0.0f;
    }
    return 0;
}

int tgpuKernelSoftmax(float *data, int count)
{
    float max_val = data[0];
    for (int i = 1; i < count; i++) {
        if (data[i] > max_val) max_val = data[i];
    }

    float sum = 0.0f;
    for (int i = 0; i < count; i++) {
        data[i] = expf(data[i] - max_val);
        sum += data[i];
    }

    for (int i = 0; i < count; i++) {
        data[i] /= sum;
    }

    return 0;
}

/* ── 向量运算 ───────────────────────────────────── */

int tgpuKernelVectorAdd(float *c, const float *a, const float *b,
                        uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        c[i] = a[i] + b[i];
    }
    return 0;
}

float tgpuKernelDot(const float *a, const float *b, int count)
{
    float result = 0.0f;
    for (int i = 0; i < count; i++) {
        result += a[i] * b[i];
    }
    return result;
}

/* ── 卷积 ───────────────────────────────────────── */

int tgpuKernelConv2D(
    const float *input, const float *kernel,
    float *output,
    int H, int W,
    int KH, int KW,
    int C_in, int C_out)
{
    int OH = H - KH + 1;
    int OW = W - KW + 1;

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
