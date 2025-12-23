# 实验 4 编译与运行说明

## 依赖
- qemu-system-riscv64
- riscv64-unknown-elf 工具链

## 编译
在 `exp4/kernel` 目录：
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
qemu-system-riscv64 -machine virt -nographic -bios default -kernel kernel.elf
```

## 清理
```bash
make clean
```

## 说明
- 本实验引入中断/陷阱处理与页表，运行输出在串口。