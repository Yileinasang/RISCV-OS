# 实验 5 编译与运行说明

## 依赖
- qemu-system-riscv64
- riscv64-unknown-elf 工具链

## 编译
在 `exp5/kernel` 目录：
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
- 本实验实现进程管理与调度（轮转/优先级基础）。