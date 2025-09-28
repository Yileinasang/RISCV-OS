CROSS    ?= riscv64-unknown-elf
CC       := $(CROSS)-gcc
LD       := $(CROSS)-gcc
OBJCOPY  := $(CROSS)-objcopy
OBJDUMP  := $(CROSS)-objdump

CFLAGS   := -march=rv64imac -mabi=lp64 -mcmodel=medany \
            -Wall -Wextra -O2 -ffreestanding -fno-builtin -fno-pic -nostdlib
LDFLAGS  := -nostdlib -Wl,-T,kernel/kernel.ld

SRC_ASM  := kernel/entry.S
SRC_C    := kernel/main.c kernel/uart.c
OBJ      := build/entry.o build/main.o build/uart.o

all: build kernel.elf

build:
	mkdir -p build

build/%.o: kernel/%.S
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJ) kernel/kernel.ld
	$(LD) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

clean:
	rm -rf build kernel.elf

run: kernel.elf
	qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf

inspect: kernel.elf
	$(OBJDUMP) -h kernel.elf
	$(CROSS)-nm kernel.elf | grep -E "(_start|kernel_main|__bss|__stack|kernel_)"
