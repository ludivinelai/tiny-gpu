/*
 * tiny-gpu UMD (User-Mode Driver)
 *
 * 封装 ioctl / mmap，提供用户态对 KMD 的简洁接口。
 * 编译为静态库 libtgpu.a。
 *
 * API:
 *   tgpu_open()       - 打开设备
 *   tgpu_close()      - 关闭设备
 *   tgpu_launch()     - 提交计算任务
 *   tgpu_get_status() - 查询设备状态
 */

#ifndef _TGPU_UMD_H
#define _TGPU_UMD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 操作码 (与 QEMU 设备 / KMD 一致) */
#define TGPU_OP_COPY  0
#define TGPU_OP_VADD  1
#define TGPU_OP_VMUL  2

/* 设备文件路径 */
#define TGPU_DEVICE_PATH  "/dev/tiny-gpu"

/* launch 参数 */
typedef struct {
    uint64_t src_user;   /* 源 buffer (用户态地址) */
    uint64_t dst_user;   /* 目标 buffer (用户态地址) */
    uint32_t size;       /* 数据大小 (bytes) */
    uint32_t opcode;     /* 操作码 */
    uint32_t result;     /* [out] 计算结果 */
} tgpu_launch_args_t;

/* 设备信息 */
typedef struct {
    uint32_t status;
    uint32_t pad;
} tgpu_info_t;

/* ── API ──────────────────────────────────────── */

/** 打开设备，返回 fd，失败返回 -1 */
int tgpu_open(void);

/** 关闭设备 */
void tgpu_close(int fd);

/** 提交计算任务，返回 0 成功，<0 失败 */
int tgpu_launch(int fd, tgpu_launch_args_t *args);

/** 获取设备状态 */
int tgpu_get_status(int fd, tgpu_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* _TGPU_UMD_H */
