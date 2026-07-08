#!/bin/sh
# Alpine 自动安装脚本 - 在 VM 启动时执行

echo "=== tiny-gpu: Alpine auto-install ==="

# 等待设备就绪
sleep 2

# 分区并格式化
echo -e "o\nn\np\n1\n\n\nw" | fdisk /dev/vda 2>/dev/null
mkfs.ext4 -F /dev/vda 2>/dev/null
mount /dev/vda /mnt

# 挂载 ISO 作为包源
mkdir -p /media/cdrom
mount -t iso9660 /dev/vdb2 /media/cdrom 2>/dev/null || mount /dev/vdb /media/cdrom 2>/dev/null

# 设置 APK 仓库
mkdir -p /mnt/etc/apk
cp /etc/apk/repositories /mnt/etc/apk/repositories 2>/dev/null

# 安装基础系统 + 编译工具
apk add --root /mnt --initdb alpine-base linux-lts build-base git linux-lts-dev 2>/dev/null

# 配置
echo "alpine" > /mnt/etc/hostname
echo "nameserver 1.1.1.1" > /mnt/etc/resolv.conf
echo "/dev/vda / ext4 defaults 0 0" > /mnt/etc/fstab
echo "ttyAMA0::respawn:/sbin/getty -L ttyAMA0 115200 vt100" > /mnt/etc/inittab
echo "root::0:0:root:/root:/bin/sh" > /mnt/etc/passwd

# 设置开机自动登录
sed -i 's/^ttyAMA0.*/ttyAMA0::respawn:\/sbin\/getty -L ttyAMA0 115200 vt100 -n -l \/bin\/sh/' /mnt/etc/inittab 2>/dev/null

echo "=== Installation complete ==="
sync
