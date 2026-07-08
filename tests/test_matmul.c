/*
 * tiny-gpu 矩阵乘法测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../runtime/tgpu_runtime.h"
#include "../ops/operators.h"

#define M  64
#define N  64
#define K  64
#define TOLERANCE  0.01f

static int test_relu(void)
{
    printf("[TEST] ReLU (N=%d)... ", 8);

    float data[] = {-2.0f, -1.0f, 0.0f, 1.0f, 3.0f, -0.5f, 2.5f, -10.0f};
    float expected[] = {0.0f, 0.0f, 0.0f, 1.0f, 3.0f, 0.0f, 2.5f, 0.0f};

    tgpuReLU(data, 8);

    for (int i = 0; i < 8; i++) {
        if (fabsf(data[i] - expected[i]) > TOLERANCE) {
            printf("FAIL at %d: got %f, expected %f\n",
                   i, data[i], expected[i]);
            return 1;
        }
    }
    printf("PASS\n");
    return 0;
}

static int test_softmax(void)
{
    printf("[TEST] Softmax (N=%d)... ", 5);

    float data[] = {1.0f, 2.0f, 3.0f, 4.0f, 1.0f};
    float sum = 0.0f;

    tgpuSoftmax(data, 5);

    for (int i = 0; i < 5; i++)
        sum += data[i];

    /* sum should be ~1.0 */
    if (fabsf(sum - 1.0f) > TOLERANCE) {
        printf("FAIL: sum=%f, expected 1.0\n", sum);
        return 1;
    }

    /* values should be decreasing from index 3 */
    if (data[3] < data[0] || data[3] < data[2]) {
        printf("FAIL: unexpected softmax values\n");
        return 1;
    }

    printf("PASS\n");
    return 0;
}

static int test_sgemm(void)
{
    printf("[TEST] SGEMM (%dx%dx%d)... ", M, N, K);

    float *A = malloc(M * K * sizeof(float));
    float *B = malloc(K * N * sizeof(float));
    float *C = malloc(M * N * sizeof(float));
    float *C_tiled = malloc(M * N * sizeof(float));

    /* 初始化: A = 行号, B = 列号 */
    for (int i = 0; i < M; i++)
        for (int k = 0; k < K; k++)
            A[i * K + k] = (float)(i + k);

    for (int k = 0; k < K; k++)
        for (int j = 0; j < N; j++)
            B[k * N + j] = (float)(k + j);

    /* 朴素版本 */
    tgpuSgemm(A, B, C, M, N, K);

    /* tiled 版本 */
    tgpuSgemmTiled(A, B, C_tiled, M, N, K);

    /* 比较两个版本 */
    for (int i = 0; i < M * N; i++) {
        if (fabsf(C[i] - C_tiled[i]) > TOLERANCE) {
            printf("FAIL at %d: naive=%f, tiled=%f\n",
                   i, C[i], C_tiled[i]);
            goto cleanup;
        }
    }

    printf("PASS\n");

cleanup:
    free(A); free(B); free(C); free(C_tiled);
    return 0;
}

int main(void)
{
    printf("\n╔══════════════════════════════════╗\n");
    printf("║   tiny-gpu: MatMul Test Suite   ║\n");
    printf("╚══════════════════════════════════╝\n\n");

    test_relu();
    test_softmax();
    test_sgemm();

    printf("\nAll operator tests complete!\n");
    return 0;
}
