# ========== 
# Toolchain
# ==========
CROSS_COMPILE ?= riscv64-unknown-elf-
CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

CFLAGS  = -march=rv64imac -mabi=lp64 -mcmodel=medany -ffreestanding -nostdlib -nostartfiles -O2
LDFLAGS = -T linker.ld -nostdlib

# ==========
# QEMU
# ==========
QEMU         ?= qemu-system-riscv64
QEMU_MACHINE ?= virt

# ==========
# Files
# ==========
SRCS = src/kernel.c src/uart.c src/fs.c src/start.S
OBJS = $(SRCS:.c=.o)
OBJS := $(OBJS:.S=.o)

all: kernel.elf kernel.bin

kernel.elf: $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

# ==========
# Run in QEMU
# ==========
run: kernel.elf
	$(QEMU) -machine $(QEMU_MACHINE) -nographic -bios none -kernel $<

# ==========
# Utilities
# ==========
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) kernel.elf kernel.bin

.PHONY: all clean run
