# 实验 2 编译与运行说明

## 依赖
- qemu-system-riscv64
- riscv64-unknown-elf 工具链

## 编译
在 `exp2` 目录：
```bash
make
```
生成：`kernel.elf`

## 运行
```bash
make run
```
等价命令：
```bash
qemu-system-riscv64 -machine virt -bios none -kernel kernel.elf -serial mon:stdio
```

## 清理
```bash
make clean
```

## 备注
- 本实验引入 `console/printf`，串口以 `-serial mon:stdio` 方式输出到终端。