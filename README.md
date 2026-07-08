# tiny-gpu

**全链路自研类 CUDA 驱动栈** — 从 KMD → UMD → Runtime → 算子，在 QEMU 虚拟设备上跑通完整通路。

## 架构

```
┌──────────────────────────────────────┐
│  ops/    算子层 (tgpuKernel*)         │  ← 矩阵乘法, ReLU, Softmax
├──────────────────────────────────────┤
│  runtime/    Runtime API (tgpu*)     │  ← tgpuMalloc / tgpuLaunch
├──────────────────────────────────────┤
│  umd/    用户态驱动 (tgpu_*)         │  ← ioctl / mmap 封装
├──────────────────────────────────────┤
│  kmd/    内核态驱动 (tgpu.ko)        │  ← char device / PCI driver
├──────────────────────────────────────┤
│  device/ QEMU 虚拟设备               │  ← 模拟的 "GPU"
└──────────────────────────────────────┘
```

## API 命名规范

| 前缀 | 层 | 示例 |
|---|---|---|
| `tgpu_` | UMD（用户态驱动） | `tgpu_open()`, `tgpu_launch()` |
| `tgpu` | Runtime（设备管理） | `tgpuInit()`, `tgpuMalloc()`, `tgpuLaunch()` |
| `tgpuKernel` | 算子（计算逻辑） | `tgpuKernelSgemm()`, `tgpuKernelReLU()` |

## 快速开始

```bash
# 1. 编译 QEMU (带 tiny-gpu 设备)
make qemu

# 2. 创建 Linux VM
make vm

# 3. 编译全栈
make all

# 4. 测试
make test
```

## 虚拟设备规格 (tiny-gpu PCI device)

- **Vendor ID:** 0x1D0F (虚构)
- **Device ID:** 0x0001
- **BAR0:** 4KB MMIO 区域，10 个 32-bit 寄存器

| 偏移 | 寄存器 | 读写 | 用途 |
|---|---|---|---|
| 0x00 | REG_CMD | W | 命令寄存器 |
| 0x04 | REG_STATUS | R | 状态 (0=idle, 1=busy, 2=done) |
| 0x08 | REG_SRC_ADDR_LO | R/W | 源地址低 32 位 |
| 0x0C | REG_SRC_ADDR_HI | R/W | 源地址高 32 位 |
| 0x10 | REG_DST_ADDR_LO | R/W | 目标地址低 32 位 |
| 0x14 | REG_DST_ADDR_HI | R/W | 目标地址高 32 位 |
| 0x18 | REG_SIZE | R/W | 数据大小 (bytes) |
| 0x1C | REG_OPCODE | R/W | 操作码 (0=COPY, 1=VADD, 2=VMUL) |
| 0x20 | REG_RESULT | R | 结果 |
| 0x24 | REG_INT_MASK | R/W | 中断掩码 |

## License

MIT
