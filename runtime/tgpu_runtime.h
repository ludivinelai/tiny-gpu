/*
 * tiny-gpu Runtime API (tgpu_runtime.h)
 *
 * 命名规范: tgpu* — Runtime 层（设备管理、内存、提交任务）
 *            tgpuKernel* — 算子层（具体计算逻辑，见 ops/operators.h）
 *
 * 典型用法：
 *
 *   #include "tgpu_runtime.h"
 *   #include "../ops/operators.h"
 *
 *   float *a, *b, *c;
 *   tgpuInit();
 *   tgpuMalloc((void**)&a, N * sizeof(float));
 *   tgpuMemcpy(a, host_a, N * sizeof(float));   // H2D
 *
 *   tgpuKernelVectorAdd(c, a, b, N);             // 算子
 *   tgpuMemcpy(host_c, c, N * sizeof(float));    // D2H
 *
 *   tgpuFree(a); tgpuFree(b); tgpuFree(c);
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

/** 初始化 tiny-gpu 设备，返回 0 成功 */
int tgpuInit(void);

/** 关闭 tiny-gpu 设备 */
void tgpuShutdown(void);

/* ── 内存管理 ──────────────────────────────────── */

/** 分配设备内存（当前为 host malloc，将来可改为设备显存） */
int tgpuMalloc(void **ptr, size_t size);

/** 释放设备内存 */
void tgpuFree(void *ptr);

/** 拷贝数据（Host ↔ Device 双向，当前均为 host 内存直接操作） */
int tgpuMemcpy(void *dst, const void *src, size_t size);

/* ── 任务提交 ──────────────────────────────────── */

/**
 * 向设备提交一个计算任务。
 * 当前设备仅支持固定操作码，未来支持可编程 kernel。
 *
 * opcode: 0=COPY, 1=VADD, 2=VMUL
 */
int tgpuLaunch(void *dst, const void *src, uint32_t size, uint32_t opcode);

/** 等待所有已提交的任务完成 */
int tgpuSync(void);

/* ── 工具 ──────────────────────────────────────── */

/** 获取设备状态字符串: "idle" / "busy" / "done" / "unknown" */
const char* tgpuGetStatusStr(void);

/** 获取驱动版本 */
void tgpuGetVersion(int *major, int *minor);

#ifdef __cplusplus
}
#endif

#endif /* _TGPU_RUNTIME_H */
