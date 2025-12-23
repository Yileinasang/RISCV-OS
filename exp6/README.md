# 实验 6 编译与运行说明

## 依赖
- qemu-system-riscv64
- riscv64-unknown-elf 工具链
- 主机 `gcc`（构建 `mkfs`）
- `perl`（生成 `usys.S`）

## 编译
在 `exp6/kernel` 目录：
```bash
make
```
生成：`kernel.elf`，并可构建用户态接口 `usys.S/usys.o`

## 运行
```bash
make run
```
自动生成并挂载 `fs.img` 后运行：
```bash
qemu-system-riscv64 -machine virt -nographic -bios default -kernel kernel.elf \
  -global virtio-mmio.force-legacy=false \
  -drive file=fs.img,if=none,format=raw,id=x0 \
  -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
```

## 清理
```bash
make clean
```

## 说明
- 本实验为系统调用与基础文件系统支撑。