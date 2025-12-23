# 定义交叉编译工具链前缀
CROSS = riscv64-unknown-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJDUMP = $(CROSS)objdump
NM = $(CROSS)nm
QEMU = qemu-system-riscv64

# Host 工具（用于构建 mkfs）
HOSTCC = gcc
HOSTCFLAGS = -O2 -Wall
MKFS = mkfs

# 编译选项
CFLAGS = -Wall -O2 -ffreestanding -nostdlib -nostartfiles -mcmodel=medany
LDFLAGS = -T kernel.ld

# 目标文件
OBJS = entry.o uart.o console.o printf.o spinlock.o kalloc.o string.o vm.o trap.o kernelvec.o sbi.o proc.o swtch.o swtest.o bio.o file.o fs.o sleeplock.o sysfile.o virtio_disk.o log.o snprintf.o sysproc.o trampoline.o usys.o start.o

# 最终生成的内核文件
KERNEL = kernel.elf

# 默认目标
all: $(KERNEL)

# 编译汇编文件
entry.o: entry.S
	$(CC) $(CFLAGS) -c $< -o $@

kernelvec.o: kernelvec.S
	$(CC) $(CFLAGS) -c $< -o $@

swtch.o: swtch.S
	$(CC) $(CFLAGS) -c $< -o $@

trampoline.o: trampoline.S
	$(CC) $(CFLAGS) -c $< -o $@

usys.S : usys.pl
	perl usys.pl > usys.S

usys.o : usys.S
	$(CC) $(CFLAGS) -c -o usys.o usys.S

# 编译 C 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 链接生成内核文件
$(KERNEL): $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

# 构建 host 上的 mkfs 工具（用于生成 fs.img）
$(MKFS): mkfs.c
	$(HOSTCC) $(HOSTCFLAGS) -I. -o $(MKFS) mkfs.c

# 生成文件系统镜像 fs.img（使用当前目录下的 mkfs）
fs.img: $(MKFS)
	./$(MKFS) fs.img

# 运行内核
run: $(KERNEL) fs.img usys.o
	$(QEMU) -machine virt -nographic -bios default -kernel $(KERNEL) -global virtio-mmio.force-legacy=false -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
# 清理生成的文件
clean:
	rm -f $(OBJS) $(KERNEL) fs.img usys.S usys.o