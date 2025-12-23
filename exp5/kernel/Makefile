CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld
QEMU = qemu-system-riscv64

CFLAGS = -Wall -O2 -ffreestanding -nostdlib -nostartfiles -mcmodel=medany
LDFLAGS = -T kernel.ld

OBJS = entry.o uart.o console.o printf.o start.o spinlock.o kalloc.o string.o vm.o trap.o kernelvec.o sbi.o proc.o swtch.o swtest.o 
KERNEL = kernel.elf

all: $(KERNEL)

entry.o: entry.S
	$(CC) $(CFLAGS) -c $< -o $@

kernelvec.o: kernelvec.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

run: $(KERNEL)
	$(QEMU) -machine virt -nographic -bios default -kernel $(KERNEL)

clean:
	rm -f $(OBJS) $(KERNEL)
