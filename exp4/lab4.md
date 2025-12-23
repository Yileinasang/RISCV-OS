# 实验报告（exp4：中断处理与时钟管理）

## 实验概述
- 实验目标：在 S 模式实现中断/异常处理，完成时钟中断与异常分发。
- 完成情况：`stvec` 向量、`kerneltrap` 分发、SBI 定时器驱动、异常测试通过。
- 开发环境：Ubuntu 22.04；riscv64-unknown-elf-gcc 12.x；qemu-system-riscv64 7.x。

## 关键函数实现
- 向量入口（`kernel/kernelvec.S`）：保存寄存器、跳转 C、恢复后 `sret`。
```asm
kernelvec:
  addi sp, sp, -256
  sd ra, 0(sp)
  sd a0, 8(sp)
  call kerneltrap
  ld ra, 0(sp)
  ld a0, 8(sp)
  addi sp, sp, 256
  sret
```
- 中断分发（`kernel/trap.c`）：判断异常/中断并调用回调。
```c
void kerneltrap(void){ uint64 scause=r_scause(); if(scause&(1ULL<<63)) interrupt_dispatch(scause&0xff); else handle_exception(scause);} 
```
- 定时器回调（`kernel/trap.c`）：重新设置下一次触发。
```c
static void timer_interrupt(void){ ticks++; sbi_set_timer(r_time()+1000000);} 
```

## 难点突破
- CSR 保存最小化：只保存调用破坏的寄存器；若支持用户态需扩展 trapframe。
- `sepc` 安全返回：异常可能调整 `sepc` 跳过故障指令，确保 4 字节对齐。

## 源码理解与对比
- 相同：S 模式 trap、SBI 定时器、向量入口 + C 分发与 xv6 接近。
- 不同：当前仅内核态自陷，未处理用户态返回与外部/软件中断；回调表更易扩展。

## 测试情况
- 功能：`make && make run`，观察 tick 打印与异常日志。
- 边界：调节定时间隔；注释 `sret` 前恢复代码观察异常情况。

## 思考题与回答
- 中断设计：
  - M→S 委托原因：M 模式完成特权配置后，通过 `medeleg/mideleg` 委托到 S 模式，以实现隔离。
  - 优先级系统：维护优先级与嵌套规则，结合 PLIC/CLINT，避免低优先级阻塞高优先级。
- 性能考虑：
  - 开销：现场保存/恢复、缓存与页表命中损耗、设备访问；优化为精简上下文、向量入口、合并事件。
  - 高频影响：过高频率会抢占 CPU，调节时钟、批处理事件、限定处理时间。
- 可靠性：
  - 安全性：避免长时间持锁，保证可重入或禁用重入，边界检查防止栈溢出。
  - 错误处理：记录日志与报警，严重错误 `panic` 或软复位。
- 扩展性：
  - 更多中断源：驱动注册与 PLIC 动态配置，抽象通用 ISR。
  - 动态路由：按核负载分配 IRQ，支持亲和迁移。
- 实时性：
  - 延迟测量：`rdcycle/rdtime` 统计入口到完成差值。
  - 满足实时：设置不可屏蔽高优先级通道、限制处理时间、与调度协同控制最坏延迟。
