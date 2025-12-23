# 实验 7 编译与运行说明

## 依赖
- qemu-system-riscv64
- riscv64-unknown-elf 工具链
- 主机 `gcc`（构建 `mkfs`）
- `perl`（生成 `usys.S`）

## 编译
在 `exp7/kernel` 目录：
```bash
make
```
生成：`kernel.elf` 与 `mkfs`

## 运行
```bash
make run
```
会生成并挂载 `fs.img`。

## 清理
```bash
make clean
```

## 说明
- 本实验实现完整日志文件系统、缓冲区与驱动；支持并发与崩溃恢复测试。