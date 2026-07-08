/*
 * tiny-gpu Runtime API 实现
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
        return 0;

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

/* ── 内存管理 ──────────────────────────────────── */

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

int tgpuMemcpy(void *dst, const void *src, size_t size)
{
    memcpy(dst, src, size);
    return 0;
}

/* ── 任务提交 ──────────────────────────────────── */

int tgpuLaunch(void *dst, const void *src, uint32_t size, uint32_t opcode)
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

int tgpuSync(void)
{
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
