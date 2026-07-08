/*
 * tiny-gpu QEMU Virtual Device (QEMU 11.x compatible)
 *
 * 一个最小化的 PCI 计算加速器设备模型。
 * 暴露 MMIO 寄存器，在 QEMU 进程内用 CPU 完成计算。
 *
 * 用法：qemu-system-aarch64 -device tiny-gpu
 */

#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"
#include "hw/pci/pci_ids.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qom/object.h"
#include "system/address-spaces.h"
#include "hw/core/qdev.h"

#define TYPE_TINY_GPU "tiny-gpu"
OBJECT_DECLARE_SIMPLE_TYPE(TinyGPUState, TINY_GPU)

#define TINY_GPU_VENDOR_ID    0x1D0F
#define TINY_GPU_DEVICE_ID    0x0001
#define TINY_GPU_BAR0_SIZE    0x1000
#define TINY_GPU_COMPUTE_DELAY_NS 1000

/* MMIO 寄存器偏移 (8字节对齐，避免冲突) */
enum {
    REG_CMD          = 0x00,
    REG_STATUS       = 0x04,
    REG_SRC_ADDR_LO  = 0x08,
    REG_SRC_ADDR_HI  = 0x0C,
    REG_DST_ADDR_LO  = 0x10,
    REG_DST_ADDR_HI  = 0x14,
    REG_SIZE         = 0x18,
    REG_OPCODE       = 0x1C,
    REG_RESULT       = 0x20,
    REG_INT_MASK     = 0x24,
    REG_MAX          = 0x28,
};

/* 操作码 */
enum {
    OP_COPY = 0,
    OP_VADD = 1,
    OP_VMUL = 2,
};

struct TinyGPUState {
    PCIDevice   pdev;
    MemoryRegion mmio;
    uint32_t    regs[REG_MAX / 4];
    QEMUTimer  *compute_timer;
};

/* ── 计算逻辑 ──────────────────────────────────── */

static void tiny_gpu_do_compute(TinyGPUState *s)
{
    uint64_t src_pa, dst_pa;
    uint32_t size, opcode;
    uint32_t result = 0;

    src_pa  = ((uint64_t)s->regs[REG_SRC_ADDR_HI / 4] << 32)
            |  s->regs[REG_SRC_ADDR_LO / 4];
    dst_pa  = ((uint64_t)s->regs[REG_DST_ADDR_HI / 4] << 32)
            |  s->regs[REG_DST_ADDR_LO / 4];
    size    =  s->regs[REG_SIZE / 4];
    opcode  =  s->regs[REG_OPCODE / 4];

    if (size == 0 || size > (1 << 20)) {
        s->regs[REG_STATUS / 4] = 0xFFFFFFFF;
        return;
    }

    uint8_t *src = g_malloc(size);
    uint8_t *dst = g_malloc(size);

    address_space_read(&address_space_memory, src_pa,
                       MEMTXATTRS_UNSPECIFIED, src, size);

    switch (opcode) {
    case OP_COPY:
        memcpy(dst, src, size);
        result = size;
        break;

    case OP_VADD: {
        uint32_t count = size / sizeof(uint32_t);
        uint32_t *s32 = (uint32_t *)src;
        uint32_t *d32 = (uint32_t *)dst;
        for (uint32_t i = 0; i < count; i++) {
            d32[i] = s32[i] + (i % 10);
        }
        result = count;
        break;
    }

    case OP_VMUL: {
        uint32_t count = size / sizeof(uint32_t);
        uint32_t *s32 = (uint32_t *)src;
        uint32_t *d32 = (uint32_t *)dst;
        for (uint32_t i = 0; i < count; i++) {
            d32[i] = s32[i] * (i % 10 + 1);
        }
        result = count;
        break;
    }

    default:
        g_free(src);
        g_free(dst);
        s->regs[REG_STATUS / 4] = 0xFFFFFFFF;
        return;
    }

    address_space_write(&address_space_memory, dst_pa,
                        MEMTXATTRS_UNSPECIFIED, dst, size);

    g_free(src);
    g_free(dst);

    s->regs[REG_RESULT / 4]  = result;
    s->regs[REG_STATUS / 4]  = 2;

    if (s->regs[REG_INT_MASK / 4] & 0x1) {
        pci_irq_assert(&s->pdev);
    }
}

static void tiny_gpu_compute_timer_cb(void *opaque)
{
    tiny_gpu_do_compute((TinyGPUState *)opaque);
}

/* ── MMIO ──────────────────────────────────────── */

static uint64_t tiny_gpu_mmio_read(void *opaque, hwaddr addr,
                                   unsigned size)
{
    TinyGPUState *s = opaque;
    uint32_t val = 0;

    if (addr < REG_MAX) {
        val = s->regs[addr / 4];
    }
    return val;
}

static void tiny_gpu_mmio_write(void *opaque, hwaddr addr,
                                uint64_t val, unsigned size)
{
    TinyGPUState *s = opaque;

    if (addr >= REG_MAX) return;

    switch (addr) {
    case REG_CMD:
        if (val == 1) {
            s->regs[REG_STATUS / 4] = 1;
            s->regs[REG_CMD / 4]   = 0;
            timer_mod_ns(s->compute_timer,
                qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL)
                + TINY_GPU_COMPUTE_DELAY_NS);
        }
        break;

    case REG_SRC_ADDR_LO:
    case REG_SRC_ADDR_HI:
    case REG_DST_ADDR_LO:
    case REG_DST_ADDR_HI:
    case REG_SIZE:
    case REG_OPCODE:
    case REG_INT_MASK:
        s->regs[addr / 4] = (uint32_t)val;
        break;

    default:
        break;
    }
}

static const MemoryRegionOps tiny_gpu_mmio_ops = {
    .read  = tiny_gpu_mmio_read,
    .write = tiny_gpu_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

/* ── PCI 生命周期 ──────────────────────────────── */

static void tiny_gpu_realize(PCIDevice *pdev, Error **errp)
{
    TinyGPUState *s = TINY_GPU(pdev);

    pci_config_set_vendor_id(pdev->config, TINY_GPU_VENDOR_ID);
    pci_config_set_device_id(pdev->config, TINY_GPU_DEVICE_ID);
    pci_config_set_class(pdev->config, PCI_CLASS_OTHERS);

    memory_region_init_io(&s->mmio, OBJECT(s), &tiny_gpu_mmio_ops, s,
                          "tiny-gpu-mmio", TINY_GPU_BAR0_SIZE);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &s->mmio);

    memset(s->regs, 0, sizeof(s->regs));
    s->regs[REG_STATUS / 4] = 0;

    s->compute_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL,
                                    tiny_gpu_compute_timer_cb, s);
}

static void tiny_gpu_exit(PCIDevice *pdev)
{
    TinyGPUState *s = TINY_GPU(pdev);
    timer_del(s->compute_timer);
    timer_free(s->compute_timer);
}

static void tiny_gpu_reset_hold(Object *obj, ResetType type)
{
    TinyGPUState *s = TINY_GPU(obj);
    memset(s->regs, 0, sizeof(s->regs));
    s->regs[REG_STATUS / 4] = 0;
}

/* ── 迁移 ──────────────────────────────────────── */

static const VMStateDescription vmstate_tiny_gpu = {
    .name = "tiny-gpu",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, TinyGPUState, REG_MAX / 4),
        VMSTATE_END_OF_LIST()
    }
};

/* ── 设备注册 ──────────────────────────────────── */

static void tiny_gpu_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize   = tiny_gpu_realize;
    k->exit      = tiny_gpu_exit;
    k->vendor_id = TINY_GPU_VENDOR_ID;
    k->device_id = TINY_GPU_DEVICE_ID;
    k->class_id  = PCI_CLASS_OTHERS;

    dc->desc     = "Tiny GPU Accelerator";
    dc->vmsd     = &vmstate_tiny_gpu;
    dc->user_creatable = true;

    ResettableClass *rc = RESETTABLE_CLASS(klass);
    rc->phases.hold = tiny_gpu_reset_hold;
}

static const TypeInfo tiny_gpu_info = {
    .name          = TYPE_TINY_GPU,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(TinyGPUState),
    .class_init    = tiny_gpu_class_init,
    .interfaces    = (InterfaceInfo[]) {
        { INTERFACE_PCIE_DEVICE },
        { },
    },
};

static void tiny_gpu_register_types(void)
{
    type_register_static(&tiny_gpu_info);
}

type_init(tiny_gpu_register_types)
