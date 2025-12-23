# 实验报告（exp1：RISC-V 引导与裸机启动）

## 实验概述
- 实验目标：完成裸机引导，建立启动栈，清零 BSS，初始化 UART 并输出可观测信息。
- 完成情况：`entry.S` 启动序列、`kernel.ld` 链接布局、UART 输出 "Hello OS" 已实现并在 QEMU 运行通过。
- 开发环境：Ubuntu 22.04；riscv64-unknown-elf-gcc 12.x；qemu-system-riscv64 7.x。

## 关键函数实现
- 启动入口（`kernel/entry.S`）：设置 `sp` → 清零 BSS → 跳转 C。
```asm
_start:
  la sp, __stack_top
  la t0, __bss_start
  la t1, __bss_end
1:
  bgeu t0, t1, 2f
  sd zero, 0(t0)
  addi t0, t0, 8
  j 1b
2:
  call kernel_main
```
- UART 输出（`kernel/uart.c`）：轮询 `LSR_THRE` 后写 `THR`。
```c
static void uartputc_sync(int c){
  while((read_reg(LSR)&LSR_THRE)==0){}
  write_reg(THR,c);
}
```
- 链接脚本（`kernel/kernel.ld`）：导出栈与 BSS 符号。
```ld
PROVIDE(__bss_start = .);
.bss : { *(.bss .bss.*) *(COMMON) }
PROVIDE(__bss_end = .);
.stack : { . = . + 16K; }
PROVIDE(__stack_top = .);
```

## 难点突破
- 早期可观测性：入口先写串口字节，验证取指路径；无输出时排查基址/波特率。
- 栈与 BSS 边界：用 `objdump -h`/`nm` 验证符号，避免栈覆盖 BSS。

## 源码理解与对比
- 相同：与 xv6 一样，清 BSS、建栈、跳 C；16550 串口轮询输出。
- 不同：未启用多核与 S 模式切换；单栈固定大小，分页后需扩展 per-hart 栈与特权切换。

## 测试情况
- 基本：`make && make run`，串口显示 "Hello OS"。
- 边界：缩小栈观察溢出风险；注释 BSS 清零观察未定义行为。

## 思考题与回答
- 启动栈的设计：
  - 大小确定：按调用深度、编译器需求、可能中断与调试余量；取 16KB 便于扩展。
  - 溢出与检测：会覆盖 BSS/数据区；早期放哨兵检测，分页后在栈下方设不可访问护栏。
- BSS 段清零：
  - 不清零现象：全局/静态变量随机值导致逻辑异常或安全隐患。
  - 可省略：上层引导已保证清零或全部显式初始化，但内核实践中建议始终清零。
- 与 xv6 的对比：
  - 简化项：不处理多核 hart、未进入 S 模式、未设置页表与 trampoline。
  - 风险：开启中断/多核或需要用户态时会成为问题，后续逐步补齐。
- 错误处理：
  - UART 失败：降级为静默或回退到 SBI 输出；在出错路径 `wfi` 停驻。
  - 最小错误显示：提供 MMIO 单字节输出与 `panic()`，循环输出错误码。
