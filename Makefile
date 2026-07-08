# tiny-gpu 顶层 Makefile
# =======================
#
# 三层编译:
#   make all        - 编译所有用户态代码 (UMD + Runtime + Ops + Tests)
#   make qemu       - 编译 QEMU (带 tiny-gpu 虚拟设备)
#   make vm         - 创建 Linux VM 测试环境
#   make test       - 运行测试

.PHONY: all clean qemu vm test

CC      ?= gcc
CFLAGS  += -Wall -Wextra -O2 -g

# QEMU 配置
QEMU_SRC_DIR  ?= qemu-src
QEMU_BUILD_DIR ?= qemu-build

# ── 用户态库 ─────────────────────────────────────

all: libtgpu.a libtgpu_runtime.a libtgpu_ops.a test_basic test_matmul

# UMD
umd/tgpu.o: umd/tgpu.c umd/tgpu.h
	$(CC) $(CFLAGS) -I. -c -o $@ $<

libtgpu.a: umd/tgpu.o
	$(AR) rcs $@ $^

# Runtime
runtime/tgpu_runtime.o: runtime/tgpu_runtime.c runtime/tgpu_runtime.h umd/tgpu.h
	$(CC) $(CFLAGS) -I. -c -o $@ $<

libtgpu_runtime.a: runtime/tgpu_runtime.o
	$(AR) rcs $@ $^

# Ops
ops/operators.o: ops/operators.c ops/operators.h runtime/tgpu_runtime.h
	$(CC) $(CFLAGS) -I. -c -o $@ $<

libtgpu_ops.a: ops/operators.o
	$(AR) rcs $@ $^

# Test binaries
test_basic: tests/test_basic.c libtgpu.a libtgpu_runtime.a
	$(CC) $(CFLAGS) -I. -o $@ $< -L. -ltgpu -ltgpu_runtime

test_matmul: tests/test_matmul.c libtgpu.a libtgpu_runtime.a libtgpu_ops.a
	$(CC) $(CFLAGS) -I. -o $@ $< -L. -ltgpu -ltgpu_runtime -ltgpu_ops -lm

# ── QEMU ─────────────────────────────────────────

qemu:
	@echo "=== Building QEMU with tiny-gpu device ==="
	@echo "This requires QEMU source tree. See README.md."
	@if [ ! -d $(QEMU_SRC_DIR) ]; then \
		echo "Cloning QEMU..."; \
		git clone --depth 1 --branch stable-9.2 \
			https://gitlab.com/qemu-project/qemu.git $(QEMU_SRC_DIR); \
	fi
	@# Copy our device
	cp device/tiny-gpu.c $(QEMU_SRC_DIR)/hw/misc/
	@# Patch meson.build
	@if ! grep -q "tiny-gpu" $(QEMU_SRC_DIR)/hw/misc/meson.build; then \
		echo "" >> $(QEMU_SRC_DIR)/hw/misc/meson.build; \
		echo "# tiny-gpu" >> $(QEMU_SRC_DIR)/hw/misc/meson.build; \
		echo "system_ss.add(when: 'CONFIG_TINY_GPU', if_true: files('tiny-gpu.c'))" \
			>> $(QEMU_SRC_DIR)/hw/misc/meson.build; \
	fi
	@# Patch Kconfig
	@if ! grep -q "TINY_GPU" $(QEMU_SRC_DIR)/hw/misc/Kconfig; then \
		echo "" >> $(QEMU_SRC_DIR)/hw/misc/Kconfig; \
		echo "config TINY_GPU" >> $(QEMU_SRC_DIR)/hw/misc/Kconfig; \
		echo "    bool" >> $(QEMU_SRC_DIR)/hw/misc/Kconfig; \
		echo "    default y" >> $(QEMU_SRC_DIR)/hw/misc/Kconfig; \
		echo "    depends on PCI" >> $(QEMU_SRC_DIR)/hw/misc/Kconfig; \
	fi
	mkdir -p $(QEMU_BUILD_DIR)
	cd $(QEMU_BUILD_DIR) && ../$(QEMU_SRC_DIR)/configure \
		--target-list=x86_64-softmmu \
		--enable-debug \
		--without-default-features \
		--enable-kvm
	cd $(QEMU_BUILD_DIR) && make -j$$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# ── VM ──────────────────────────────────────────

vm: qemu
	@echo "=== Creating Linux VM ==="
	@echo "Download a minimal Linux image (e.g., Alpine) and kernel."
	@echo "Then run: ./scripts/run_qemu.sh"
	@echo ""
	@echo "Quick start with Alpine Linux:"
	@echo "  cd scripts && ./setup_vm.sh"

# ── 测试 ─────────────────────────────────────────

test: all
	@echo "=== Running tests ==="
	./test_basic
	./test_matmul

# ── 清理 ─────────────────────────────────────────

clean:
	rm -f *.o *.a
	rm -f umd/*.o umd/*.a
	rm -f runtime/*.o runtime/*.a
	rm -f ops/*.o ops/*.a
	rm -f test_basic test_matmul
	rm -rf $(QEMU_BUILD_DIR)
