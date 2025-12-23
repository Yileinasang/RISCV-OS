# 实验 8 编译与运行说明

## 依赖
- qemu-system-riscv64
- riscv64-unknown-elf 工具链
- 主机 `gcc`（构建 `mkfs`）
- `perl`（生成 `usys.S`）

## 编译
在 `exp8/kernel` 目录：
```bash
make
```
生成：`kernel.elf` 与（按需）`mkfs`

## 运行
```bash
make run
```
等价于：
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
- 本实验为优先级调度扩展，沿用文件系统以支撑测试。