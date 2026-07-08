/*
 * tiny-gpu Runtime API 实现
 *
 * 内存模型：简化设计 — "device memory" 就是 host memory。
 * 真正的数据传输通过 KMD 的 ioctl 下发到 QEMU 虚拟设备。
 */

#include "tgpu_runtime.h"
#include "../umd/tgpu.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int g_fd = -1;

/* ── 设备管理 ──────────────────────────────────── */

int tgpuInit(void)
{
    if (g_fd >= 0)
        return 0; /* 已经初始化 */

    g_fd = tgpu_open();
    if (g_fd < 0) {
        fprintf(stderr, "tgpu: failed to open %s\n", TGPU_DEVICE_PATH);
        return -1;
    }
    return 0;
}

void tgpuShutdown(void)
{
    if (g_fd >= 0) {
        tgpu_close(g_fd);
        g_fd = -1;
    }
}

/* ── 内存管理 (简化: 用户态 malloc) ────────────── */

int tgpuMalloc(void **ptr, size_t size)
{
    void *p = malloc(size);
    if (!p)
        return -1;
    memset(p, 0, size);
    *ptr = p;
    return 0;
}

void tgpuFree(void *ptr)
{
    free(ptr);
}

int tgpuMemcpyH2D(void *dst, const void *src, size_t size)
{
    memcpy(dst, src, size);
    return 0;
}

int tgpuMemcpyD2H(void *dst, const void *src, size_t size)
{
    memcpy(dst, src, size);
    return 0;
}

/* ── 算子提交 ──────────────────────────────────── */

static int tgpu_submit(void *dst, const void *src,
                       uint32_t size, uint32_t opcode)
{
    tgpu_launch_args_t args = {
        .src_user = (uint64_t)(uintptr_t)src,
        .dst_user = (uint64_t)(uintptr_t)dst,
        .size     = size,
        .opcode   = opcode,
        .result   = 0,
    };

    return tgpu_launch(g_fd, &args);
}

int tgpuVectorAdd(void *c, const void *a, const void *b,
                  uint32_t count)
{
    /* 先在 host 计算 A + B，放入 C */
    uint32_t *pa = (uint32_t *)a;
    uint32_t *pb = (uint32_t *)b;
    uint32_t *pc = (uint32_t *)c;
    uint32_t size = count * sizeof(uint32_t);

    for (uint32_t i = 0; i < count; i++)
        pc[i] = pa[i] + pb[i];

    /* 提交到虚拟设备做验证 */
    return tgpu_submit(c, c, size, TGPU_OP_VADD);
}

int tgpuVectorAddFloat(void *c, const void *a, const void *b,
                       uint32_t count)
{
    float *pa = (float *)a;
    float *pb = (float *)b;
    float *pc = (float *)c;
    uint32_t size = count * sizeof(float);

    for (uint32_t i = 0; i < count; i++)
        pc[i] = pa[i] + pb[i];

    return tgpu_submit(c, c, size, TGPU_OP_VADD);
}

int tgpuVectorMul(void *c, const void *a, const void *b,
                  uint32_t count)
{
    uint32_t *pa = (uint32_t *)a;
    uint32_t *pb = (uint32_t *)b;
    uint32_t *pc = (uint32_t *)c;
    uint32_t size = count * sizeof(uint32_t);

    for (uint32_t i = 0; i < count; i++)
        pc[i] = pa[i] * pb[i];

    return tgpu_submit(c, c, size, TGPU_OP_VMUL);
}

int tgpuDeviceCopy(void *dst, const void *src, size_t size)
{
    return tgpu_submit(dst, src, size, TGPU_OP_COPY);
}

int tgpuSync(void)
{
    /* 当前实现是同步的，无需等待 */
    return 0;
}

/* ── 工具 ──────────────────────────────────────── */

const char* tgpuGetStatusStr(void)
{
    tgpu_info_t info;
    if (tgpu_get_status(g_fd, &info) < 0)
        return "unknown";

    switch (info.status) {
    case 0:  return "idle";
    case 1:  return "busy";
    case 2:  return "done";
    default: return "error";
    }
}

void tgpuGetVersion(int *major, int *minor)
{
    *major = 0;
    *minor = 1;
}
