/*
 * tiny-gpu KMD (Kernel-Mode Driver)
 *
 * Linux PCI 驱动 + char device。
 * 编译：make -C /lib/modules/$(uname -r)/build M=$(PWD) modules
 * 加载：insmod tgpu.ko
 * 设备节点：/dev/tiny-gpu
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>

#define DRIVER_NAME     "tiny-gpu"
#define DEVICE_NAME     "tiny-gpu"

#define TGPU_VENDOR_ID  0x1D0F
#define TGPU_DEVICE_ID  0x0001

/* MMIO 寄存器偏移 (与 QEMU 设备一致) */
#define REG_CMD          0x00
#define REG_STATUS       0x04
#define REG_SRC_ADDR_LO  0x08
#define REG_SRC_ADDR_HI  0x0C
#define REG_DST_ADDR_LO  0x10
#define REG_DST_ADDR_HI  0x14
#define REG_SIZE         0x18
#define REG_OPCODE       0x1C
#define REG_RESULT       0x20
#define REG_INT_MASK     0x24

/* ioctl 命令 */
#define TGPU_IOC_MAGIC  'G'

struct tgpu_launch_args {
    __u64 src_user;    /* 用户态源 buffer 地址 */
    __u64 dst_user;    /* 用户态目标 buffer 地址 */
    __u32 size;        /* 数据大小 */
    __u32 opcode;      /* 操作码 */
    __u32 result;      /* [out] 结果 */
};

struct tgpu_info {
    __u32 status;
    __u32 pad;
};

#define TGPU_IOC_LAUNCH  _IOWR(TGPU_IOC_MAGIC, 1, struct tgpu_launch_args)
#define TGPU_IOC_INFO    _IOR (TGPU_IOC_MAGIC, 2, struct tgpu_info)

/* 设备私有数据 */
struct tgpu_dev {
    struct pci_dev      *pdev;
    void __iomem        *bar0;          /* MMIO 映射 */
    unsigned long        bar0_phys;     /* BAR0 物理地址 */
    unsigned long        bar0_size;

    struct cdev          cdev;
    struct device       *device;
    dev_t                devt;          /* 设备号 */

    wait_queue_head_t    waitq;         /* 等待中断 */
    atomic_t             irq_done;      /* 中断完成标志 */

    /* DMA buffer (用于用户态数据中转) */
    void                *dma_buf;
    dma_addr_t           dma_handle;
};

static struct class *tgpu_class;

/* ── PCI 设备表 ────────────────────────────────── */

static const struct pci_device_id tgpu_pci_ids[] = {
    { PCI_DEVICE(TGPU_VENDOR_ID, TGPU_DEVICE_ID), },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, tgpu_pci_ids);

/* ── MMIO 辅助函数 ─────────────────────────────── */

static inline void tgpu_write_reg(struct tgpu_dev *tgpu,
                                   unsigned int reg, u32 val)
{
    iowrite32(val, tgpu->bar0 + reg);
}

static inline u32 tgpu_read_reg(struct tgpu_dev *tgpu,
                                 unsigned int reg)
{
    return ioread32(tgpu->bar0 + reg);
}

/* ── 等待计算完成 (轮询 + 等待队列) ─────────────── */

static int tgpu_wait_done(struct tgpu_dev *tgpu, unsigned long timeout_ms)
{
    unsigned long timeout = msecs_to_jiffies(timeout_ms);
    int ret;

    atomic_set(&tgpu->irq_done, 0);

    /* 启用中断 */
    tgpu_write_reg(tgpu, REG_INT_MASK, 0x1);

    ret = wait_event_interruptible_timeout(
        tgpu->waitq,
        atomic_read(&tgpu->irq_done),
        timeout
    );

    /* 禁用中断 */
    tgpu_write_reg(tgpu, REG_INT_MASK, 0x0);

    if (ret == 0)
        return -ETIMEDOUT;
    if (ret < 0)
        return ret;
    return 0;
}

/* ── 中断处理 ──────────────────────────────────── */

static irqreturn_t tgpu_irq_handler(int irq, void *dev_id)
{
    struct tgpu_dev *tgpu = dev_id;

    /* 检查状态是否为 done */
    u32 status = tgpu_read_reg(tgpu, REG_STATUS);
    if (status == 2) {
        atomic_set(&tgpu->irq_done, 1);
        wake_up(&tgpu->waitq);
        return IRQ_HANDLED;
    }
    return IRQ_NONE;
}

/* ── char device: open ────────────────────────── */

static int tgpu_open(struct inode *inode, struct file *file)
{
    struct tgpu_dev *tgpu;

    tgpu = container_of(inode->i_cdev, struct tgpu_dev, cdev);
    file->private_data = tgpu;

    dev_dbg(&tgpu->pdev->dev, "device opened\n");
    return 0;
}

/* ── char device: release ─────────────────────── */

static int tgpu_release(struct inode *inode, struct file *file)
{
    struct tgpu_dev *tgpu = file->private_data;
    dev_dbg(&tgpu->pdev->dev, "device closed\n");
    return 0;
}

/* ── char device: ioctl ───────────────────────── */

static long tgpu_ioctl(struct file *file, unsigned int cmd,
                        unsigned long arg)
{
    struct tgpu_dev *tgpu = file->private_data;
    int ret = 0;

    switch (cmd) {

    case TGPU_IOC_LAUNCH: {
        struct tgpu_launch_args args;
        void *src_kern, *dst_kern;
        dma_addr_t src_dma, dst_dma;

        if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
            return -EFAULT;

        if (args.size > (1 << 20)) /* 最大 1MB */
            return -EINVAL;

        /* 分配 DMA buffer */
        src_kern = dma_alloc_coherent(&tgpu->pdev->dev, args.size,
                                      &src_dma, GFP_KERNEL);
        dst_kern = dma_alloc_coherent(&tgpu->pdev->dev, args.size,
                                      &dst_dma, GFP_KERNEL);
        if (!src_kern || !dst_kern) {
            ret = -ENOMEM;
            goto free_bufs;
        }

        /* 从用户态拷贝数据 */
        if (copy_from_user(src_kern, (void __user *)args.src_user,
                           args.size)) {
            ret = -EFAULT;
            goto free_bufs;
        }

        /* 设置寄存器 */
        tgpu_write_reg(tgpu, REG_SRC_ADDR_HI, (u32)(src_dma >> 32));
        tgpu_write_reg(tgpu, REG_SRC_ADDR_LO, (u32)(src_dma & 0xFFFFFFFF));
        tgpu_write_reg(tgpu, REG_DST_ADDR_HI, (u32)(dst_dma >> 32));
        tgpu_write_reg(tgpu, REG_DST_ADDR_LO, (u32)(dst_dma & 0xFFFFFFFF));
        tgpu_write_reg(tgpu, REG_SIZE,   args.size);
        tgpu_write_reg(tgpu, REG_OPCODE, args.opcode);

        /* 启动计算 */
        tgpu_write_reg(tgpu, REG_CMD, 1);

        /* 等待完成 */
        ret = tgpu_wait_done(tgpu, 5000);
        if (ret < 0) {
            dev_err(&tgpu->pdev->dev, "compute timed out\n");
            goto free_bufs;
        }

        /* 读结果 */
        args.result = tgpu_read_reg(tgpu, REG_RESULT);

        /* 拷贝结果回用户态 */
        if (copy_to_user((void __user *)args.dst_user, dst_kern, args.size)) {
            ret = -EFAULT;
            goto free_bufs;
        }

        if (copy_to_user((void __user *)arg, &args, sizeof(args)))
            ret = -EFAULT;

    free_bufs:
        if (src_kern)
            dma_free_coherent(&tgpu->pdev->dev, args.size,
                              src_kern, src_dma);
        if (dst_kern)
            dma_free_coherent(&tgpu->pdev->dev, args.size,
                              dst_kern, dst_dma);
        return ret;
    }

    case TGPU_IOC_INFO: {
        struct tgpu_info info;
        info.status = tgpu_read_reg(tgpu, REG_STATUS);
        info.pad = 0;
        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
            return -EFAULT;
        return 0;
    }

    default:
        return -ENOTTY;
    }
}

/* ── char device: mmap ────────────────────────── */

static int tgpu_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct tgpu_dev *tgpu = file->private_data;
    unsigned long pfn = tgpu->bar0_phys >> PAGE_SHIFT;
    size_t size = vma->vm_end - vma->vm_start;

    if (size > tgpu->bar0_size)
        return -EINVAL;

    /* 禁止缓存，确保每次读写都到硬件 */
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

    if (io_remap_pfn_range(vma, vma->vm_start, pfn, size,
                           vma->vm_page_prot))
        return -EAGAIN;

    return 0;
}

/* ── file_operations ──────────────────────────── */

static const struct file_operations tgpu_fops = {
    .owner           = THIS_MODULE,
    .open            = tgpu_open,
    .release         = tgpu_release,
    .unlocked_ioctl  = tgpu_ioctl,
    .mmap            = tgpu_mmap,
};

/* ── PCI probe ────────────────────────────────── */

static int tgpu_pci_probe(struct pci_dev *pdev,
                          const struct pci_device_id *id)
{
    struct tgpu_dev *tgpu;
    int ret;

    dev_info(&pdev->dev, "tiny-gpu: probing device\n");

    /* 启用 PCI 设备 */
    ret = pci_enable_device(pdev);
    if (ret)
        return ret;

    ret = pci_request_regions(pdev, DRIVER_NAME);
    if (ret)
        goto disable_device;

    /* 设置 DMA mask */
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if (ret) {
        ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
        if (ret)
            goto release_regions;
    }

    /* 分配设备结构 */
    tgpu = devm_kzalloc(&pdev->dev, sizeof(*tgpu), GFP_KERNEL);
    if (!tgpu) {
        ret = -ENOMEM;
        goto release_regions;
    }
    pci_set_drvdata(pdev, tgpu);
    tgpu->pdev = pdev;

    /* 映射 BAR0 */
    tgpu->bar0_phys = pci_resource_start(pdev, 0);
    tgpu->bar0_size = pci_resource_len(pdev, 0);
    tgpu->bar0 = ioremap(tgpu->bar0_phys, tgpu->bar0_size);
    if (!tgpu->bar0) {
        ret = -ENOMEM;
        goto release_regions;
    }

    /* 注册中断 */
    init_waitqueue_head(&tgpu->waitq);
    atomic_set(&tgpu->irq_done, 0);

    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    if (ret < 0) {
        /* MSI 不可用，尝试传统中断 */
        ret = request_irq(pdev->irq, tgpu_irq_handler,
                          IRQF_SHARED, DRIVER_NAME, tgpu);
    } else {
        ret = request_irq(pci_irq_vector(pdev, 0), tgpu_irq_handler,
                          0, DRIVER_NAME, tgpu);
    }
    if (ret) {
        dev_warn(&pdev->dev,
                 "IRQ not available, using polling fallback\n");
        /* 无中断也能工作 (polling) */
    }

    /* 注册 char device */
    tgpu->devt = MKDEV(MAJOR(0), 0); /* 动态分配 */
    ret = alloc_chrdev_region(&tgpu->devt, 0, 1, DEVICE_NAME);
    if (ret)
        goto free_irq;

    cdev_init(&tgpu->cdev, &tgpu_fops);
    tgpu->cdev.owner = THIS_MODULE;
    ret = cdev_add(&tgpu->cdev, tgpu->devt, 1);
    if (ret)
        goto unreg_region;

    tgpu->device = device_create(tgpu_class, &pdev->dev,
                                 tgpu->devt, tgpu, DEVICE_NAME);
    if (IS_ERR(tgpu->device)) {
        ret = PTR_ERR(tgpu->device);
        goto del_cdev;
    }

    /* 重置设备 */
    tgpu_write_reg(tgpu, REG_INT_MASK, 0);

    dev_info(&pdev->dev,
             "tiny-gpu: registered %s (BAR0: 0x%lx, IRQ: %d)\n",
             DEVICE_NAME, tgpu->bar0_phys, pdev->irq);

    return 0;

del_cdev:
    cdev_del(&tgpu->cdev);
unreg_region:
    unregister_chrdev_region(tgpu->devt, 1);
free_irq:
    free_irq(pdev->irq, tgpu);
    iounmap(tgpu->bar0);
release_regions:
    pci_release_regions(pdev);
disable_device:
    pci_disable_device(pdev);
    return ret;
}

/* ── PCI remove ───────────────────────────────── */

static void tgpu_pci_remove(struct pci_dev *pdev)
{
    struct tgpu_dev *tgpu = pci_get_drvdata(pdev);

    device_destroy(tgpu_class, tgpu->devt);
    cdev_del(&tgpu->cdev);
    unregister_chrdev_region(tgpu->devt, 1);
    free_irq(pdev->irq, tgpu);
    pci_free_irq_vectors(pdev);
    iounmap(tgpu->bar0);
    pci_release_regions(pdev);
    pci_disable_device(pdev);

    dev_info(&pdev->dev, "tiny-gpu: device removed\n");
}

/* ── PCI 驱动结构 ──────────────────────────────── */

static struct pci_driver tgpu_pci_driver = {
    .name     = DRIVER_NAME,
    .id_table = tgpu_pci_ids,
    .probe    = tgpu_pci_probe,
    .remove   = tgpu_pci_remove,
};

/* ── 模块加载/卸载 ─────────────────────────────── */

static int __init tgpu_init(void)
{
    int ret;

    /* 创建设备类 */
    tgpu_class = class_create(DRIVER_NAME);
    if (IS_ERR(tgpu_class))
        return PTR_ERR(tgpu_class);

    ret = pci_register_driver(&tgpu_pci_driver);
    if (ret) {
        class_destroy(tgpu_class);
        return ret;
    }

    pr_info("tiny-gpu: kernel driver loaded\n");
    return 0;
}

static void __exit tgpu_exit(void)
{
    pci_unregister_driver(&tgpu_pci_driver);
    class_destroy(tgpu_class);
    pr_info("tiny-gpu: kernel driver unloaded\n");
}

module_init(tgpu_init);
module_exit(tgpu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("tiny-gpu project");
MODULE_DESCRIPTION("Tiny GPU Kernel-Mode Driver");
