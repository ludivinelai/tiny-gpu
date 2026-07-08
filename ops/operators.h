/*
 * tiny-gpu 算子头文件
 */

#ifndef _TGPU_OPS_H
#define _TGPU_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 矩阵乘法 C = A * B */
int tgpuSgemm(const float *A, const float *B, float *C,
              int M, int N, int K);

/* 矩阵乘法 (tiled) */
int tgpuSgemmTiled(const float *A, const float *B, float *C,
                   int M, int N, int K);

/* ReLU 激活 */
int tgpuReLU(float *data, int count);

/* Softmax */
int tgpuSoftmax(float *data, int count);

/* 向量加法 (通过 QEMU 设备) */
int tgpuOpVectorAdd(float *c, const float *a, const float *b,
                    uint32_t count);

/* 向量点积 */
float tgpuOpDot(const float *a, const float *b, int count);

/* 卷积 2D (朴素实现, 用于验证) */
int tgpuOpConv2D(
    const float *input, const float *kernel,
    float *output,
    int H, int W, int KH, int KW,
    int C_in, int C_out);

#ifdef __cplusplus
}
#endif

#endif /* _TGPU_OPS_H */
