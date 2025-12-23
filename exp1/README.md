# 实验 1 编译与运行说明

## 依赖
- qemu-system-riscv64
- riscv64-unknown-elf 工具链（gcc/ld/objdump 等）
- Linux (已在 Ubuntu 上验证)

## 编译
在 `exp1` 目录：

```bash
make
```

生成产物：`kernel.elf`

## 运行
```bash
make run
```
等价于：
```bash
qemu-system-riscv64 -machine virt -nographic -bios none -kernel kernel.elf
```

## 清理
```bash
make clean
```

## 常见问题
- 未找到 `qemu-system-riscv64`：请通过发行版包管理器安装（例如 Ubuntu: `sudo apt install qemu-system-misc`）。
- 未找到 `riscv64-unknown-elf-gcc`：参考 riscv 工具链安装指南或使用预编译包。