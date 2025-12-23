# 实验报告（exp5：进程管理与调度）

## 实验概述
- 实验目标：实现进程创建/退出/等待与轮转/优先级调度。
- 完成情况：`alloc/create/exit/wait`、`scheduler_priority/rotate`、`sleep/wakeup/yield` 自测通过。
- 开发环境：Ubuntu 22.04；riscv64-unknown-elf-gcc 12.x；qemu-system-riscv64 7.x。

## 关键函数实现
- 进程分配（`kernel/proc.c`）：初始化 pid、trapframe、页表与文件表。
```c
static struct proc* alloc_process(void){
  acquire(&pid_lock); int pid=next_pid++; release(&pid_lock);
  struct proc *p=alloc_unused_proc(); p->pid=pid; p->state=USED;
  p->trapframe=(struct trapframe*)kalloc(); p->pagetable=proc_pagetable(p);
  return p;
}
```
- 调度器（`kernel/proc.c`）：选择 RUNNABLE 并切换。
```c
void scheduler_priority(void){ for(;;){ struct proc *p=pick_highest_prio(); if(p){ p->state=RUNNING; swtch(&sched_ctx,&p->context);} } }
```
- 睡眠与唤醒（`kernel/proc.c`）：等待资源时释放锁并阻塞。
```c
void sleep(void *chan, struct spinlock *lk){ acquire(&p->lock); release(lk); p->chan=chan; p->state=SLEEPING; swtch(&p->context,&sched_ctx); p->chan=0; release(&p->lock); acquire(lk);} 
```

## 难点突破
- 状态机一致性：`sleep/wakeup` 必须持有 proc 锁修改状态，防止丢失唤醒。
- 公平性：优先级衰减避免长期占用；轮转模式保证同级公平。

## 源码理解与对比
- 与 xv6：接口一致（`yield/sleep/wakeup`，`wait/exit`）。
- 差异：当前仅内核线程；提供两种调度器入口用于对比。

## 测试情况
- 功能：`make && make run`，观察轮转顺序与优先级抢占。
- 边界：超过 `NPROC` 创建失败；持锁调用 `sleep` 断言防死锁。

## 思考题与回答
- 进程模型：
  - 选择原因：`struct proc` + 内核栈与上下文，简单直观，便于调度与恢复。
  - 轻量级线程：`struct thread` 共享地址空间与文件表，独立栈与上下文，调度粒度为线程。
- 调度策略：
  - 轮转公平性：同优先级按时间片轮转，结合 aging 防饥饿。
  - 实时调度：引入 EDF/RM 参数与限制阻塞路径。
- 性能优化：
  - fork()：写时复制与懒分配降低开销。
  - 切换：精简保存集、减少频繁 `yield`、增大时间片、减小锁冲突。
- 资源管理：
  - 限制：最大内存/文件/CPU 时间配额，超限拒绝或回收。
  - 泄漏：`exit()` 回收页表、文件、管道与子进程；守护扫描孤儿资源。
- 扩展性：
  - 多核：每核就绪队列 + 工作窃取，考虑亲和与共享资源。
  - 负载均衡：周期性或按需迁移任务，避免热点核过载。
