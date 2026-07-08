#!/bin/bash
# tiny-gpu QEMU 启动脚本
#
# 用法: ./run_qemu.sh [kernel] [rootfs]
#
# 需要:
#   1. Linux kernel (bzImage)
#   2. Root filesystem (rootfs.img)
#
# 快速获取 Alpine Linux:
#   curl -O https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/x86_64/alpine-virt-3.21.0-x86_64.iso

set -e

QEMU_BUILD="./qemu-build"
KERNEL="${1:-vmlinuz}"
ROOTFS="${2:-rootfs.img}"
KERNEL_APPEND="console=ttyS0 earlyprintk=serial"

# 如果给了 ISO，从 ISO 启动 (安装/测试)
ISO="${3:-}"

QEMU_BIN="$QEMU_BUILD/qemu-system-x86_64"

if [ ! -x "$QEMU_BIN" ]; then
    QEMU_BIN="qemu-system-x86_64"  # fallback to system QEMU
fi

echo "=== tiny-gpu QEMU launcher ==="
echo "  kernel: $KERNEL"
echo "  rootfs: $ROOTFS"
echo "  qemu:   $QEMU_BIN"

# 基础参数
QEMU_ARGS=(
    -m 512M
    -smp 2
    -nographic
    -kernel "$KERNEL"
    -append "$KERNEL_APPEND"
    -device tiny-gpu
)

# 如果有 rootfs
if [ -f "$ROOTFS" ]; then
    QEMU_ARGS+=(
        -drive file="$ROOTFS",format=raw,if=virtio
    )
fi

# 如果有 ISO
if [ -n "$ISO" ] && [ -f "$ISO" ]; then
    QEMU_ARGS+=(
        -cdrom "$ISO"
        -boot d
    )
    if [ ! -f "$ROOTFS" ]; then
        # 创建临时磁盘
        qemu-img create -f raw tmp-disk.img 4G
        QEMU_ARGS+=(
            -drive file=tmp-disk.img,format=raw,if=virtio
        )
    fi
fi

exec "$QEMU_BIN" "${QEMU_ARGS[@]}"
