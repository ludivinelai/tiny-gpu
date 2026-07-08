# tiny-gpu UMD Makefile
# 编译用户态库 libtgpu.a

CC      ?= gcc
AR      ?= ar
CFLAGS  += -Wall -Wextra -O2 -I.

libtgpu.a: tgpu.o
	$(AR) rcs $@ $^

tgpu.o: tgpu.c tgpu.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.a
