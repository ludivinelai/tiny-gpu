# tiny-gpu KMD Makefile
# 在 Linux guest 中编译:
#   make -C /lib/modules/$(uname -r)/build M=$(PWD) modules

obj-m += tgpu.o

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD       := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
