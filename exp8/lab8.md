# 实验报告（exp8：优先级调度）

## 实验概述
- 实验目标：实现基于优先级的调度器，支持动态调整与老化（aging），在保证高优先级优先的同时避免饥饿。
- 完成情况：扩展 `proc` 字段，修改 `scheduler` 选择策略，时钟中断驱动 aging，新增 `setpriority/getpriority` 接口；自测通过。
- 开发环境：Ubuntu 22.04；riscv64-unknown-elf-gcc 12.x；qemu-system-riscv64 7.x。

## 关键函数实现
- 优先级选择（`kernel/proc.c`）：线性扫描挑选最高优先级的 RUNNABLE，等优先级轮转。
```c
static struct proc* pick_highest_prio(void){
  struct proc *best = 0;
  for(struct proc *p=proc; p<&proc[NPROC]; p++)
    if(p->state==RUNNABLE && (!best || p->priority>best->priority))
      best = p;
  return best;
}
```
- aging 更新（`kernel/trap.c`/时钟回调）：对 RUNNABLE 进程累计等待并适度提升。
```c
void clockintr(void){
  for(struct proc *p=proc; p<&proc[NPROC]; p++)
    if(p->state==RUNNABLE && ++p->wait_ticks >= AGING_THRESHOLD){
      if(p->priority < MAX_PRIO) p->priority++;
      p->wait_ticks = 0;
    }
}
```
- 优先级接口（`kernel/sysproc.c`）：用户态动态设置。
```c
uint64 sys_setpriority(void){
  int pid, prio; argint(0,&pid); argint(1,&prio);
  return setpriority(pid, prio);
}
```

## 难点突破
- 防饥饿与稳定性：仅对 RUNNABLE 累计等待，睡眠态不加龄；阈值与步长调优避免频繁抖动。
- 公平性与吞吐：等优先级轮转+时间片，限制高优先级霸占；抢占与临界区协调避免锁内被打断。

## 源码理解与对比
- 与 RR：在 RR 基础上引入权重控制与老化，保持实现简单、易验证；选择 O(N) 扫描而非多级队列以减小改动面。
- 可演进：多级反馈队列（MLFQ）/每核就绪队列、权重映射到时间配额，改进交互响应与扩展性。

## 测试情况
- 功能：创建不同优先级的循环打印任务，观察高优先级优先运行；时间片用尽后被重调度。
- 公平性：长时间运行下低优先级任务通过老化获得 CPU，避免饥饿。
- 接口：`setpriority/getpriority` 动态调整立即生效，边界参数（<0 或 >MAX）按策略钳位或拒绝。
