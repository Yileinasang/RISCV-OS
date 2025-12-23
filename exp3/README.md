# 实验 3 编译与运行说明

## 依赖
- qemu-system-riscv64
- riscv64-unknown-elf 工具链

## 编译
在 `exp3` 目录：
```bash
make
```
生成：`kernel.elf`

## 运行
```bash
make run
```
等价：
```bash
qemu-system-riscv64 -machine virt -bios none -kernel kernel.elf -serial mon:stdio
```

## 清理
```bash
make clean
```

## 调试
```bash
make inspect
```
查看段信息与符号（`_start/kernel_main/__bss/__stack` 等）。