# tiny-gpu

**全链路自研类 CUDA 驱动栈** — 从 KMD → UMD → Runtime → 算子，在 QEMU 虚拟设备上跑通完整通路。

```
┌──────────────────────────────────┐
│  ops/    算子层 (matmul, relu…)  │  ← 你写的 "kernel"
├──────────────────────────────────┤
│  runtime/    Runtime API         │  ← tgpuMalloc / tgpuLaunch
├──────────────────────────────────┤
│  umd/    用户态驱动 (libtgpu)    │  ← ioctl / mmap 封装
├──────────────────────────────────┤
│  kmd/    内核态驱动 (tgpu.ko)    │  ← char device / PCI driver
├──────────────────────────────────┤
│  device/ QEMU 虚拟设备            │  ← 模拟的 "GPU"
└──────────────────────────────────┘
```

## 架构

| 层 | 运行位置 | 语言 | 定位 |
|---|---|---|---|
| Operator | 用户态 | C | 具体计算逻辑 |
| Runtime | 用户态 | C | `tgpuMalloc`, `tgpuMemcpy`, `tgpuLaunch` |
| UMD | 用户态 | C | 封装 ioctl，做 buffer 管理 |
| KMD | 内核态 | C | PCI 驱动 + char device |
| 虚拟设备 | QEMU | C | MMIO 寄存器 + 中断 |

## 虚拟设备规格 (tiny-gpu PCI device)

- **Vendor ID:** 0x1D0F (虚构)
- **Device ID:** 0x0001
- **BAR0:** 4KB MMIO 区域，8 个 32-bit 寄存器

| 偏移 | 寄存器 | 读写 | 用途 |
|---|---|---|---|
| 0x00 | REG_CMD | W | 命令寄存器 |
| 0x04 | REG_STATUS | R | 状态 (0=idle, 1=busy, 2=done, -1=error) |
| 0x08 | REG_SRC_ADDR | R/W | 源数据物理地址 |
| 0x0C | REG_DST_ADDR | R/W | 目标数据物理地址 |
| 0x10 | REG_SIZE | R/W | 数据大小 (bytes) |
| 0x14 | REG_OPCODE | R/W | 操作码 (0=copy, 1=add, 2=mul) |
| 0x18 | REG_RESULT | R | 结果 (仅部分操作) |
| 0x1C | REG_INT_MASK | R/W | 中断掩码 |

## 操作码

| Code | 操作 | 说明 |
|---|---|---|
| 0 | COPY | dst[i] = src[i] |
| 1 | VADD | dst[i] = src[i] + arg |
| 2 | VMUL | dst[i] = src[i] * arg |

## 开发环境

- **Host:** macOS (QEMU 运行虚拟设备)
- **Guest:** Linux (Buildroot / Ubuntu) 运行 KMD + UMD + Runtime
- **QEMU:** 修改源码加入 tiny-gpu 虚拟设备

## 快速开始

```bash
# 1. 编译 QEMU (带 tiny-gpu 设备)
make qemu

# 2. 创建 Linux VM
make vm

# 3. 编译全栈
make

# 4. 测试
make test
```

## License

MIT
