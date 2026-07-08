/*
 * tiny-gpu Runtime API (tgpu.h)
 *
 * 类 CUDA 编程接口，运行在用户态，底层调用 UMD。
 *
 * 典型用法：
 *
 *   #include "tgpu_runtime.h"
 *
 *   float *a, *b, *c;
 *   int N = 1024;
 *
 *   tgpuInit();
 *   tgpuMalloc((void**)&a, N * sizeof(float));
 *   tgpuMalloc((void**)&b, N * sizeof(float));
 *   tgpuMalloc((void**)&c, N * sizeof(float));
 *
 *   tgpuMemcpyH2D(a, host_a, N * sizeof(float));
 *   tgpuMemcpyH2D(b, host_b, N * sizeof(float));
 *
 *   tgpuVectorAdd(c, a, b, N);
 *   tgpuSync();
 *
 *   tgpuMemcpyD2H(host_c, c, N * sizeof(float));
 *
 *   tgpuFree(a);
 *   tgpuFree(b);
 *   tgpuFree(c);
 *   tgpuShutdown();
 */

#ifndef _TGPU_RUNTIME_H
#define _TGPU_RUNTIME_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 设备管理 ──────────────────────────────────── */

/** 初始化 tiny-gpu 设备 */
int tgpuInit(void);

/** 关闭 tiny-gpu 设备 */
void tgpuShutdown(void);

/* ── 内存管理 ──────────────────────────────────── */

/** 分配 "device memory" (实际是用户态 malloc, 模拟显存) */
int tgpuMalloc(void **ptr, size_t size);

/** 释放 "device memory" */
void tgpuFree(void *ptr);

/** 拷贝数据 Host → Device */
int tgpuMemcpyH2D(void *dst, const void *src, size_t size);

/** 拷贝数据 Device → Host */
int tgpuMemcpyD2H(void *dst, const void *src, size_t size);

/* ── 算子 API ──────────────────────────────────── */

/** 向量加法: C = A + B (uint32_t) */
int tgpuVectorAdd(void *c, const void *a, const void *b,
                  uint32_t count);

/** 向量加法: C = A + B (float) */
int tgpuVectorAddFloat(void *c, const void *a, const void *b,
                       uint32_t count);

/** 向量乘法: C = A * B (uint32_t) */
int tgpuVectorMul(void *c, const void *a, const void *b,
                  uint32_t count);

/** 内存拷贝 (device → device, 通过 QEMU 设备) */
int tgpuDeviceCopy(void *dst, const void *src, size_t size);

/** 等待所有提交的任务完成 */
int tgpuSync(void);

/* ── 工具 ──────────────────────────────────────── */

/** 获取设备状态字符串 */
const char* tgpuGetStatusStr(void);

/** 获取驱动版本信息 */
void tgpuGetVersion(int *major, int *minor);

#ifdef __cplusplus
}
#endif

#endif /* _TGPU_RUNTIME_H */
