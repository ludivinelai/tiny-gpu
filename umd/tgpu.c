/*
 * tiny-gpu UMD 实现
 */

#include "tgpu.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>

/* ioctl 命令 (与 KMD 一致) */
#define TGPU_IOC_MAGIC  'G'

struct tgpu_launch_ioctl {
    uint64_t src_user;
    uint64_t dst_user;
    uint32_t size;
    uint32_t opcode;
    uint32_t result;
};

struct tgpu_info_ioctl {
    uint32_t status;
    uint32_t pad;
};

#define TGPU_IOC_LAUNCH  _IOWR(TGPU_IOC_MAGIC, 1, struct tgpu_launch_ioctl)
#define TGPU_IOC_INFO    _IOR (TGPU_IOC_MAGIC, 2, struct tgpu_info_ioctl)

int tgpu_open(void)
{
    return open(TGPU_DEVICE_PATH, O_RDWR);
}

void tgpu_close(int fd)
{
    if (fd >= 0)
        close(fd);
}

int tgpu_launch(int fd, tgpu_launch_args_t *args)
{
    struct tgpu_launch_ioctl kargs = {
        .src_user = args->src_user,
        .dst_user = args->dst_user,
        .size     = args->size,
        .opcode   = args->opcode,
        .result   = 0,
    };
    int ret;

    ret = ioctl(fd, TGPU_IOC_LAUNCH, &kargs);
    if (ret < 0)
        return -errno;

    args->result = kargs.result;
    return 0;
}

int tgpu_get_status(int fd, tgpu_info_t *info)
{
    struct tgpu_info_ioctl kinfo;
    int ret;

    ret = ioctl(fd, TGPU_IOC_INFO, &kinfo);
    if (ret < 0)
        return -errno;

    info->status = kinfo.status;
    info->pad = 0;
    return 0;
}
