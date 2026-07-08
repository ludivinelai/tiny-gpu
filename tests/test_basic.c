/*
 * tiny-gpu 基础测试
 *
 * 测试整个通路：Runtime → UMD → KMD → QEMU 虚拟设备
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../runtime/tgpu_runtime.h"

#define TEST_SIZE  256

static int test_vector_add(void)
{
    printf("[TEST] vector_add (N=%d)... ", TEST_SIZE);

    uint32_t *a, *b, *c;
    uint32_t host_a[TEST_SIZE], host_b[TEST_SIZE], host_c[TEST_SIZE];

    /* 准备数据 */
    for (int i = 0; i < TEST_SIZE; i++) {
        host_a[i] = i;
        host_b[i] = i * 2;
    }

    /* 分配 "device" 内存 */
    tgpuMalloc((void**)&a, TEST_SIZE * sizeof(uint32_t));
    tgpuMalloc((void**)&b, TEST_SIZE * sizeof(uint32_t));
    tgpuMalloc((void**)&c, TEST_SIZE * sizeof(uint32_t));

    /* 拷贝到 device */
    tgpuMemcpyH2D(a, host_a, TEST_SIZE * sizeof(uint32_t));
    tgpuMemcpyH2D(b, host_b, TEST_SIZE * sizeof(uint32_t));

    /* 提交计算 */
    int ret = tgpuVectorAdd(c, a, b, TEST_SIZE);
    if (ret < 0) {
        printf("FAIL (tgpuVectorAdd returned %d)\n", ret);
        goto cleanup;
    }

    /* 读回结果 */
    tgpuMemcpyD2H(host_c, c, TEST_SIZE * sizeof(uint32_t));

    /* 验证 */
    for (int i = 0; i < TEST_SIZE; i++) {
        uint32_t expected = i + i * 2;
        if (host_c[i] != expected) {
            printf("FAIL at index %d: got %u, expected %u\n",
                   i, host_c[i], expected);
            goto cleanup;
        }
    }

    printf("PASS\n");

cleanup:
    tgpuFree(a);
    tgpuFree(b);
    tgpuFree(c);
    return 0;
}

static int test_vector_mul(void)
{
    printf("[TEST] vector_mul (N=%d)... ", TEST_SIZE);

    uint32_t *a, *b, *c;
    uint32_t host_a[TEST_SIZE], host_b[TEST_SIZE], host_c[TEST_SIZE];

    for (int i = 0; i < TEST_SIZE; i++) {
        host_a[i] = i + 1;
        host_b[i] = 3;
    }

    tgpuMalloc((void**)&a, TEST_SIZE * sizeof(uint32_t));
    tgpuMalloc((void**)&b, TEST_SIZE * sizeof(uint32_t));
    tgpuMalloc((void**)&c, TEST_SIZE * sizeof(uint32_t));

    tgpuMemcpyH2D(a, host_a, TEST_SIZE * sizeof(uint32_t));
    tgpuMemcpyH2D(b, host_b, TEST_SIZE * sizeof(uint32_t));

    int ret = tgpuVectorMul(c, a, b, TEST_SIZE);
    if (ret < 0) {
        printf("FAIL (tgpuVectorMul returned %d)\n", ret);
        goto cleanup;
    }

    tgpuMemcpyD2H(host_c, c, TEST_SIZE * sizeof(uint32_t));

    for (int i = 0; i < TEST_SIZE; i++) {
        uint32_t expected = (i + 1) * 3;
        if (host_c[i] != expected) {
            printf("FAIL at index %d: got %u, expected %u\n",
                   i, host_c[i], expected);
            goto cleanup;
        }
    }

    printf("PASS\n");

cleanup:
    tgpuFree(a);
    tgpuFree(b);
    tgpuFree(c);
    return 0;
}

static int test_device_copy(void)
{
    printf("[TEST] device_copy (N=%d)... ", TEST_SIZE);

    uint8_t *src, *dst;
    uint8_t host_src[TEST_SIZE], host_dst[TEST_SIZE];

    for (int i = 0; i < TEST_SIZE; i++)
        host_src[i] = (uint8_t)(i & 0xFF);

    tgpuMalloc((void**)&src, TEST_SIZE);
    tgpuMalloc((void**)&dst, TEST_SIZE);

    tgpuMemcpyH2D(src, host_src, TEST_SIZE);

    int ret = tgpuDeviceCopy(dst, src, TEST_SIZE);
    if (ret < 0) {
        printf("FAIL (tgpuDeviceCopy returned %d)\n", ret);
        goto cleanup;
    }

    tgpuMemcpyD2H(host_dst, dst, TEST_SIZE);

    for (int i = 0; i < TEST_SIZE; i++) {
        if (host_dst[i] != host_src[i]) {
            printf("FAIL at index %d: got %u, expected %u\n",
                   i, host_dst[i], host_src[i]);
            goto cleanup;
        }
    }

    printf("PASS\n");

cleanup:
    tgpuFree(src);
    tgpuFree(dst);
    return 0;
}

int main(void)
{
    printf("\n╔══════════════════════════════════╗\n");
    printf("║   tiny-gpu: Basic Test Suite    ║\n");
    printf("╚══════════════════════════════════╝\n\n");

    if (tgpuInit() < 0) {
        printf("SKIP: /dev/tiny-gpu not available\n");
        printf("      (expected on host - run inside Linux VM)\n");
        return 0;
    }

    printf("device status: %s\n\n", tgpuGetStatusStr());

    test_vector_add();
    test_vector_mul();
    test_device_copy();

    printf("\nAll tests complete!\n");
    tgpuShutdown();
    return 0;
}
