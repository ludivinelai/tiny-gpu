/*
 * tiny-gpu 基础测试
 *
 * 测试 Runtime（设备管理 + 内存 + tgpuLaunch） → UMD → KMD → QEMU 虚拟设备
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../runtime/tgpu_runtime.h"

#define TEST_SIZE  256

static int test_device_init(void)
{
    printf("[TEST] device_init... ");
    if (tgpuInit() < 0) {
        printf("SKIP (no /dev/tiny-gpu)\n");
        return -1;
    }
    printf("PASS (%s)\n", tgpuGetStatusStr());
    return 0;
}

static int test_launch_vadd(void)
{
    printf("[TEST] tgpuLaunch(VADD, N=%d)... ", TEST_SIZE);

    uint32_t *src, *dst;
    uint32_t host_src[TEST_SIZE], host_dst[TEST_SIZE];

    for (int i = 0; i < TEST_SIZE; i++)
        host_src[i] = i;

    tgpuMalloc((void**)&src, TEST_SIZE * sizeof(uint32_t));
    tgpuMalloc((void**)&dst, TEST_SIZE * sizeof(uint32_t));

    tgpuMemcpy(src, host_src, TEST_SIZE * sizeof(uint32_t));

    int ret = tgpuLaunch(dst, src, TEST_SIZE * sizeof(uint32_t), 1 /* VADD */);
    if (ret < 0) {
        printf("FAIL (launch returned %d)\n", ret);
        goto cleanup;
    }

    tgpuMemcpy(host_dst, dst, TEST_SIZE * sizeof(uint32_t));

    for (int i = 0; i < TEST_SIZE; i++) {
        if (host_dst[i] != host_src[i] + (i % 10)) {
            printf("FAIL at %d: got %u, expected %u\n",
                   i, host_dst[i], host_src[i] + (i % 10));
            goto cleanup;
        }
    }

    printf("PASS\n");

cleanup:
    tgpuFree(src);
    tgpuFree(dst);
    return 0;
}

static int test_launch_vmul(void)
{
    printf("[TEST] tgpuLaunch(VMUL, N=%d)... ", TEST_SIZE);

    uint32_t *src, *dst;
    uint32_t host_src[TEST_SIZE], host_dst[TEST_SIZE];

    for (int i = 0; i < TEST_SIZE; i++)
        host_src[i] = i + 1;

    tgpuMalloc((void**)&src, TEST_SIZE * sizeof(uint32_t));
    tgpuMalloc((void**)&dst, TEST_SIZE * sizeof(uint32_t));

    tgpuMemcpy(src, host_src, TEST_SIZE * sizeof(uint32_t));

    int ret = tgpuLaunch(dst, src, TEST_SIZE * sizeof(uint32_t), 2 /* VMUL */);
    if (ret < 0) {
        printf("FAIL (launch returned %d)\n", ret);
        goto cleanup;
    }

    tgpuMemcpy(host_dst, dst, TEST_SIZE * sizeof(uint32_t));

    for (int i = 0; i < TEST_SIZE; i++) {
        if (host_dst[i] != host_src[i] * (i % 10 + 1)) {
            printf("FAIL at %d: got %u, expected %u\n",
                   i, host_dst[i], host_src[i] * (i % 10 + 1));
            goto cleanup;
        }
    }

    printf("PASS\n");

cleanup:
    tgpuFree(src);
    tgpuFree(dst);
    return 0;
}

static int test_launch_copy(void)
{
    printf("[TEST] tgpuLaunch(COPY, N=%d)... ", TEST_SIZE);

    uint8_t *src, *dst;
    uint8_t host_src[TEST_SIZE], host_dst[TEST_SIZE];

    for (int i = 0; i < TEST_SIZE; i++)
        host_src[i] = (uint8_t)(i & 0xFF);

    tgpuMalloc((void**)&src, TEST_SIZE);
    tgpuMalloc((void**)&dst, TEST_SIZE);

    tgpuMemcpy(src, host_src, TEST_SIZE);

    int ret = tgpuLaunch(dst, src, TEST_SIZE, 0 /* COPY */);
    if (ret < 0) {
        printf("FAIL (launch returned %d)\n", ret);
        goto cleanup;
    }

    tgpuMemcpy(host_dst, dst, TEST_SIZE);

    for (int i = 0; i < TEST_SIZE; i++) {
        if (host_dst[i] != host_src[i]) {
            printf("FAIL at %d: got %u, expected %u\n",
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
    printf("\n=== tiny-gpu: Runtime Test Suite ===\n\n");

    if (test_device_init() < 0) {
        printf("(run inside Linux VM with /dev/tiny-gpu)\n");
        return 0;
    }

    printf("\n");
    test_launch_vadd();
    test_launch_vmul();
    test_launch_copy();

    printf("\nAll runtime tests complete!\n");
    tgpuShutdown();
    return 0;
}
