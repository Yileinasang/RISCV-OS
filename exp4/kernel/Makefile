# 定义交叉编译工具链前缀
CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJDUMP = $(CROSS)objdump
NM = $(CROSS)nm
QEMU = qemu-system-riscv64

# 编译选项
CFLAGS = -Wall -O2 -ffreestanding -nostdlib -nostartfiles -mcmodel=medany
LDFLAGS = -T kernel.ld

# 目标文件
OBJS = entry.o uart.o console.o printf.o start.o spinlock.o kalloc.o string.o vm.o trap.o kernelvec.o sbi.o proc.o swtch.o swtest.o 

# 最终生成的内核文件
KERNEL = kernel.elf

# 默认目标
all: $(KERNEL)

# 编译汇编文件
entry.o: entry.S
	$(CC) $(CFLAGS) -c $< -o $@

kernelvec.o: kernelvec.S
	$(CC) $(CFLAGS) -c $< -o $@

# 编译 C 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 链接生成内核文件
$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# 运行内核
run: $(KERNEL)
	$(QEMU) -machine virt -nographic -bios default -kernel $(KERNEL)
# 清理生成的文件
clean:
	rm -f $(OBJS) $(KERNEL)