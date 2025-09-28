CROSS    ?= riscv64-unknown-elf
CC       := $(CROSS)-gcc
LD       := $(CROSS)-gcc
OBJDUMP  := $(CROSS)-objdump
NM       := $(CROSS)-nm

CFLAGS   := -march=rv64imac -mabi=lp64 -mcmodel=medany \
            -Wall -Wextra -O2 -ffreestanding -fno-builtin -fno-pic -nostdlib
LDFLAGS  := -nostdlib -Wl,-T,kernel/kernel.ld

OBJ := build/entry.o build/main.o build/uart.o build/console.o build/printf.o

all: kernel.elf

build:
	mkdir -p build

build/entry.o: kernel/entry.S | build
	$(CC) $(CFLAGS) -c $< -o $@

build/main.o: kernel/main.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/uart.o: kernel/uart.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/console.o: kernel/console.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/printf.o: kernel/printf.c | build
	$(CC) $(CFLAGS) -c $< -o $@

kernel.elf: $(OBJ)
	$(LD) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

clean:
	rm -rf build kernel.elf

run: kernel.elf
	qemu-system-riscv64 -machine virt -bios none -kernel kernel.elf -serial mon:stdio

inspect: kernel.elf
	$(OBJDUMP) -h kernel.elf
	$(NM) kernel.elf | grep -E "(_start|kernel_main|__bss|__stack)"
