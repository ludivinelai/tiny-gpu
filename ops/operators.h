/*
 * tiny-gpu 算子: 矩阵乘法 / ReLU / Softmax / 向量运算
 *
 * 命名规范: tgpuKernel* — 算子层 (区别于 Runtime 的 tgpu*)
 */

#ifndef _TGPU_OPS_H
#define _TGPU_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 矩阵乘法 C = A * B */
int tgpuKernelSgemm(const float *A, const float *B, float *C,
                    int M, int N, int K);

/* 矩阵乘法 (tiled) */
int tgpuKernelSgemmTiled(const float *A, const float *B, float *C,
                         int M, int N, int K);

/* ReLU 激活 */
int tgpuKernelReLU(float *data, int count);

/* Softmax */
int tgpuKernelSoftmax(float *data, int count);

/* 向量加法 (通过 QEMU 设备) */
int tgpuKernelVectorAdd(float *c, const float *a, const float *b,
                        uint32_t count);

/* 向量点积 */
float tgpuKernelDot(const float *a, const float *b, int count);

/* 卷积 2D (朴素实现, 用于验证) */
int tgpuKernelConv2D(
    const float *input, const float *kernel,
    float *output,
    int H, int W, int KH, int KW,
    int C_in, int C_out);

#ifdef __cplusplus
}
#endif

#endif /* _TGPU_OPS_H */
