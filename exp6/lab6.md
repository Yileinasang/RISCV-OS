# 实验报告（exp6：系统调用）

## 实验概述
- 实验目标：完成系统调用入口、分发与文件/进程相关 syscall，实现最小可用内核服务。
- 完成情况：`handle_syscall`、`sys_exit/getpid/fork/wait`、文件类 syscall、FS/日志/virtio 初始化与自测通过。
- 开发环境：Ubuntu 22.04；riscv64-unknown-elf-gcc 12.x；qemu-system-riscv64 7.x。

## 关键函数实现
- syscall 分发（`kernel/trap.c`）：读取 `a7` 并写回返回值。
```c
void handle_syscall(void){ struct proc *p=current_proc(); uint64 num=p->trapframe->a7; uint64 ret=syscalls[num]?syscalls[num]():-1; p->trapframe->a0=ret; }
```
- fork 复制（`kernel/sysproc.c`）：分配新进程并复制页表。
```c
uint64 sys_fork(void){ struct proc *np=alloc_process(); if(copy_pagetable(np->pagetable,myproc()->pagetable)<0) return -1; *(np->trapframe)=*(myproc()->trapframe); np->trapframe->a0=0; make_runnable(np); return np->pid; }
```
- 文件读写（`kernel/sysfile.c`）：统一入口检查与 `fileread/write`。
```c
uint64 sys_read(void){ int fd; uint64 p; int n; argfd(0,&fd); argaddr(1,&p); argint(2,&n); return fileread(myproc()->ofile[fd],p,n); }
```

## 难点突破
- 参数安全：统一 `copyin/copyout` 校验用户指针与长度。
- 资源一致性：失败回滚，确保文件描述符/页表/inode 不泄漏。

## 源码理解与对比
- 相同：syscall 表驱动、inode/日志/virtio 结构与 xv6 兼容。
- 不同：主要在内核自测触发 syscall，用户态加载/exec 尚未接入；`bcache_reset` 等辅助用于崩溃恢复测试。

## 测试情况
- 功能：`make && make run`，运行基本/参数/安全/性能四类自测。
- 边界：无效 fd/指针/长度返回 -1 不 panic；fork 资源不足失败。

## 思考题与回答
- 设计权衡：
  - 数量：以最小集合为核心，随需求扩展，避免 API 膨胀。
  - 安全：每个调用明确权限与资源边界，输入校验优先。
- 性能优化：
  - 开销：陷入/返回、参数复制、页表切换与缓存失效。
  - 减少切换：批处理/合并调用、减少 copyin/copyout、共享内存或映射页。
- 安全考虑：
  - 防滥用：权限检查、配额与速率限制。
  - 参数传递：统一拷贝接口与合法性校验，防止越界与竞态。
- 扩展性：
  - 新增：在表注册实现，遵循错误码与阻塞语义。
  - 兼容：ID 与语义稳定，必要时版本化或能力探测。
- 错误处理：
  - 失败：返回统一错误码，保证内核状态一致性（原子性/回滚）。
  - 报告：提供详细错误码与诊断信息便于定位。
