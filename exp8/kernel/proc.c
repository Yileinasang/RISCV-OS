#include "def.h"
#include "riscv.h"
#include "memlayout.h"

extern char trampoline[];

struct spinlock wait_lock;
struct spinlock pid_lock;
int next_pid = 1;
// 调度器上下文
struct context sched_ctx; 

int alloc_pid(void) {
    int pid;

    // 简单递增，使用原子操作避免重复pid
    acquire(&pid_lock);
    pid = next_pid;
    next_pid = next_pid + 1;
    release(&pid_lock);

    return pid;
}

void proc_mapstacks(pagetable_t kpgtbl) {
    struct proc *p;
  
    for (p = proc_table; p < &proc_table[NPROC]; p++) {
        char *pa = alloc_page();
        if (pa == 0)
            panic("kalloc");
        unsigned long va = KSTACK((int) (p - proc_table));
        map_region(kpgtbl, va, (unsigned long)pa, PGSIZE, PTE_R | PTE_W);
    }
}

void proc_init(void) {
    // 初始化pid锁和wait锁
    initlock(&pid_lock, "pid_lock");
    initlock(&wait_lock, "wait_lock");
  
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc_table[i];
        // 初始化进程表项
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        p->kstack = KSTACK((int) (p - proc_table));
        p->priority = 0;
    }
}

pagetable_t proc_pagetable(struct proc *p) {
    pagetable_t pagetable;

    // 创建新的页表
    pagetable = create_pagetable();
    if (pagetable == 0)
        return 0;

    // 映射trampoline段代码
    if (map_page(pagetable, TRAMPOLINE, (unsigned long)trampoline,
                    PGSIZE, PTE_R | PTE_X) < 0) {
        destroy_pagetable(pagetable);
        return 0;
    }

    // 映射trapframe页面
    if (map_page(pagetable, TRAPFRAME, (unsigned long)(p->trapframe), 
                    PGSIZE,PTE_R | PTE_W) < 0){
        unmap_page(pagetable, TRAMPOLINE, 1, 0);
        destroy_pagetable(pagetable);
        return 0;
    }

    return pagetable;
}

void proc_freepagetable(pagetable_t pagetable, unsigned long sz) {
    unmap_page(pagetable, TRAMPOLINE, 1, 0);
    unmap_page(pagetable, TRAPFRAME, 1, 0);
    destroy_pagetable(pagetable);
}

struct proc* alloc_process(void) {
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc_table[i];
        acquire(&p->lock);
    
        // 寻找未分配的进程表项
        if (p->state == UNUSED) {
            p->state = USED;
            p->pid = alloc_pid();

            // 分配trapframe页面
            if ((p->trapframe = (struct trapframe *)alloc_page()) == 0) {
                free_process(p);
                release(&p->lock);
                return 0;
            }

            // 创建进程页表
            p->pagetable = proc_pagetable(p);
            if (p->pagetable == 0) {
                free_process(p);
                release(&p->lock);
                return 0;
            }
            
            // 设置上下文
            memset(&p->context, 0, sizeof(p->context));
            p->context.ra = (unsigned long)forkret;
            // p->context.ra = 0;
            p->context.sp = p->kstack + PGSIZE;

            // 设置当前目录
            p->cwd = namei("/");

            p->parent = 0;

            // 返回时保持p->lock
            return p; 
        }

        release(&p->lock);
    }

    return 0;
}

void free_process(struct proc *p) {
    if (p->trapframe)
        free_page((void *)p->trapframe);
    p->trapframe = 0;
    if (p->pagetable)
        proc_freepagetable(p->pagetable, p->sz);
    p->pagetable = 0;
    p->sz = 0;
    p->pid = 0;
    p->priority = 0;
    p->parent = 0;
    p->chan = 0;
    p->xstate = 0;
    p->state = UNUSED;
}

int create_process(void (*entry)(void)) {
    struct proc *p = alloc_process();
    // 没有可用进程表项
    if (!p) 
        return -1;

    // 设置进程入口
    p->context.ra = (unsigned long)entry;
    
    // 将进程状态设置为可运行
    p->state = RUNNABLE;
    p->timeslice = 0;
    p->timetotal = 0;
    release(&p->lock);
    return p->pid;
}

void exit_process(struct proc *p, int status) {
    if (current_proc == p)
        current_proc = 0;

    // 关闭进程打开的文件
    for (int fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd]) {
            struct file *f = p->ofile[fd];
            fileclose(f);
            p->ofile[fd] = 0;
        }
    }

    // 释放进程目录的inode
    begin_op();
    iput(p->cwd);
    end_op();
    p->cwd = 0;

    if (p->parent) { 
        acquire(&wait_lock);
        wakeup(p->parent);
        release(&wait_lock);
    }

    acquire(&p->lock);
    p->xstate = status;
    p->state = ZOMBIE;
    release(&p->lock);

    // 切换回调度器
    swtch(&p->context, &sched_ctx);
}

int wait_process(unsigned long addr) {
    struct proc *pp;
    int havekids, pid;
    struct proc *p = current_proc;

    acquire(&wait_lock);

    for (;;) {
        havekids = 0;
        for (int i = 0; i < NPROC; i++) {
            pp = &proc_table[i];
            if (pp->parent == p) {
                acquire(&pp->lock);

                havekids = 1;
                if (pp->state == ZOMBIE || pp->state == RUNNABLE) {
                    pid = pp->pid;
                    if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                             sizeof(pp->xstate)) < 0) {
                        release(&pp->lock);
                        release(&wait_lock);
                        return -1;
                    }
                    free_process(pp);
                    release(&pp->lock);
                    release(&wait_lock);
                    return pid;
                }
                release(&pp->lock);
            }
        }

        if (!havekids) {
            release(&wait_lock);
            return -1;
        }

        sleep(p, &wait_lock);
    }
}

void set_proc_priority(int pid, int pri) {
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc_table[i];
        acquire(&p->lock);
        if (p->pid == pid) {
            p->priority = pri;
            release(&p->lock);
            return;
        }
        release(&p->lock);
    }
}

int get_proc_priority(int pid) {
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc_table[i];
        acquire(&p->lock);
        if (p->pid == pid) {
            int pri = p->priority ;
            release(&p->lock);
            return pri;
        }
        release(&p->lock);
    }
}

void scheduler_priority(void) {
    struct proc *p;
    struct proc *chosen;
    int highest_priority;

    for (;;) {
        intr_on();
        chosen = 0;
        highest_priority = -1;

        // 选择优先级最高的可运行进程
        for (p = proc_table; p < &proc_table[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                int pr = p->priority;
                if (pr > highest_priority) {
                    highest_priority = pr;
                    chosen = p;
                }
            }
            release(&p->lock);
        }

        // 没有可运行的进程，退出调度器
        if (!chosen)
            return;

        acquire(&chosen->lock);
        if (chosen->state == RUNNABLE) {
            // 将进程状态设置为运行中
            chosen->state = RUNNING;
            current_proc = chosen;
                
            // 降低优先级以循环调用进程
            chosen->priority -= 3;
            release(&chosen->lock);

            // 保存调度器上下文，恢复进程上下文，运行进程内容
            swtch(&sched_ctx, &chosen->context);

            // 清除当前进程
            current_proc = 0;
        }
        else
            release(&chosen->lock);
    }
}

void scheduler_rotate(void) {
    struct proc *p;
    struct proc *chosen;

    // 轮转搜索开始的位置
    int rr_idx = 0;

    for (;;) {
        intr_on();
        chosen = 0;

        // 按轮转顺序查找第一个可运行的进程
        for (int i = 0; i < NPROC; i++) {
            int idx = (rr_idx + i) % NPROC;
            p = &proc_table[idx];
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                chosen = p;
                // 更新轮转索引到下一个位置
                rr_idx = (idx + 1) % NPROC;
                release(&p->lock);
                break;
            }
            release(&p->lock);
        }

        // 没有可运行的进程，退出调度器
        if (!chosen)
            return;

        acquire(&chosen->lock);
        if (chosen->state == RUNNABLE) {
            // 将进程状态设置为运行中
            chosen->state = RUNNING;
            current_proc = chosen;
            release(&chosen->lock);

            // 切换到进程上下文，运行进程内容
            swtch(&sched_ctx, &chosen->context);

            // 清除当前进程
            current_proc = 0;
        } 
        else
            release(&chosen->lock);
    }
}

void yield(void) {
    struct proc *p = current_proc;
    acquire(&p->lock);
    p->state = RUNNABLE;
    release(&p->lock);

    // 保存进程上下文，回到调度器
    swtch(&p->context, &sched_ctx);   
}

void sleep(void *chan, struct spinlock *lk) {
    struct proc *p = current_proc;
    if (!p) {
        // 调用者不是当前进程，无法睡眠
        release(lk);
        return;
    }

    acquire(&p->lock);
    release(lk);
    p->chan = chan;
    p->state = SLEEPING;

    // 切换回调度器，需要先释放p->lock，否则调度器会死锁
    release(&p->lock);
    swtch(&p->context, &sched_ctx);

    // 唤醒后清除睡眠队列
    acquire(&p->lock);
    p->chan = 0;
    release(&p->lock);

    // 重新持有原自旋锁
    acquire(lk);
}

void wakeup(void *chan) {
    struct proc *p;
    for (p = proc_table; p < &proc_table[NPROC]; p++) {
        acquire(&p->lock);
        // 唤醒队列中所有睡眠进程
        if (p->state == SLEEPING && p->chan == chan)
            p->state = RUNNABLE;
        release(&p->lock);
    }
}

int fork_process(void) {
    int i, pid;
    struct proc *np;
    struct proc *p = current_proc;

    // 分配新的进程
    if ((np = alloc_process()) == 0)
        return -1;

    // 将父进程页表复制到子进程
    if (copy_pagetable(p->pagetable, np->pagetable, p->sz) < 0) {
        free_process(np);
        release(&np->lock);
        return -1;
    }
    np->sz = p->sz;

    // 复制保存的用户寄存器状态
    *(np->trapframe) = *(p->trapframe);

    // 子进程返回0
    np->trapframe->a0 = 0;

    // 增加文件引用计数
    for (i = 0; i < NOFILE; i++)
        if (p->ofile[i])
            np->ofile[i] = filedup(p->ofile[i]);
    np->cwd = idup(p->cwd);

    pid = np->pid;

    release(&np->lock);

    acquire(&wait_lock);
    np->parent = p;
    release(&wait_lock);

    acquire(&np->lock);
    np->state = RUNNABLE;
    release(&np->lock);

    return pid;
}

void forkret(void) {
    extern char userret[];
    struct proc *p = current_proc;

    release(&p->lock);

    prepare_return();
    unsigned long satp = MAKE_SATP(p->pagetable);
    unsigned long trampoline_userret = TRAMPOLINE + (userret - trampoline);
    ((void (*)(unsigned long))trampoline_userret)(satp);
}

void scheduler_priority_extend(int aging) {
    struct proc *p;
    struct proc *chosen;
    int highest_priority;
    int ts;
    int pid;

    for (;;) {
        intr_on();
        chosen = 0;
        highest_priority = -1;
        ts = 1000000;
        pid = 1000000;

        // 选择优先级最高的可运行进程
        for (p = proc_table; p < &proc_table[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                int pr = p->priority;
                if (pr > highest_priority) {
                    highest_priority = pr;
                    ts = p->timeslice;
                    pid = p->pid;
                    chosen = p;
                }
                else if (pr == highest_priority) {
                    // 优先级相同，选择时间片更少的进程
                    if (p->timeslice < ts) {
                        ts = p->timeslice;
                        chosen = p;
                    }
                    else if (p->timeslice == ts) {
                        // 时间片也相同，选择pid更小的进程
                        if (p->pid < pid) {
                            pid = p->pid;
                            chosen = p;
                        }
                    }
                }
            }
            release(&p->lock);
        }

        // 没有可运行的进程，退出调度器
        if (!chosen)
            return;

        // 对未被选中的进程进行老化处理
        if (aging) {
            for (p = proc_table; p < &proc_table[NPROC]; p++) {
                acquire(&p->lock);
                if (p->state == RUNNABLE && p != chosen) {
                    p->timetotal++;
                    // 最长等待时间为2
                    if (p->timetotal >= MAX_TIME) {
                        p->priority++;
                        p->timetotal = 0;
                    }
                }
                release(&p->lock);
            }
        }

        acquire(&chosen->lock);
        if (chosen->state == RUNNABLE) {
            // 将进程状态设置为运行中
            chosen->state = RUNNING;
            // 增加时间片
            chosen->timeslice++;
            current_proc = chosen;
                
            release(&chosen->lock);

            // 保存调度器上下文，恢复进程上下文，运行进程内容
            swtch(&sched_ctx, &chosen->context);

            // 清除当前进程
            current_proc = 0;
        }
        else
            release(&chosen->lock);
    }
}