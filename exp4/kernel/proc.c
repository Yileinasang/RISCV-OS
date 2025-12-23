#include "def.h"
#include "riscv.h"

struct spinlock table_lock;
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

void proc_init(void) {
    // 创建进程表自旋锁
    initlock(&table_lock, "proc_table");
  
    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc_table[i];
        // 初始化进程表项
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        p->kstack = 0;
        p->priority = 0;
    }
}

struct proc* alloc_process(void) {
    acquire(&table_lock);

    for (int i = 0; i < NPROC; i++) {
        struct proc *p = &proc_table[i];
        acquire(&p->lock);
    
        // 寻找未分配的进程表项
        if (p->state == UNUSED) {
        p->state = USED;
        p->pid = alloc_pid();

        p->kstack = alloc_page();
        if (!p->kstack) {
            // 无法分配内核栈，释放资源并返回
            p->state = UNUSED;
            p->pid = 0;
            release(&p->lock);
            release(&table_lock);
            return 0;
        }

        // 设置上下文
        unsigned long sp = (unsigned long)p->kstack + PGSIZE;
        p->context.sp = sp;
        p->context.ra = 0;
        release(&table_lock);

        // 返回时保持p->lock
        return p; 
        }

        release(&p->lock);
    }

    release(&table_lock);
    return 0;
}

void free_process(struct proc *p) {
    if (p->kstack) {
        free_page(p->kstack);
        p->kstack = 0;
    }
    p->state = UNUSED;
    p->pid = 0;
    p->priority = 0;
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
    release(&p->lock);
    return p->pid;
}

int exit_count = 0;
void exit_process(struct proc *p, int status) {
    acquire(&p->lock);
    p->xstate = status;
    p->state = ZOMBIE;
    release(&p->lock);

    if (current_proc == p)
        current_proc = 0;
    exit_count++;

    // 唤醒队列中的所有进程
    wakeup((void*)p);

    // 切换回调度器
    swtch(&p->context, &sched_ctx);
}

int wait_process(int *status) {
    acquire(&table_lock);
    for (;;) {
        for (int i = 0; i < NPROC; i++) {
            struct proc *p = &proc_table[i];
            acquire(&p->lock);

            // 找到一个可回收的进程
            if (p->state == RUNNABLE || p->state == ZOMBIE) {
                int pid = p->pid;
                if (status) {
                    // 获取退出状态
                    *status = p->xstate;
                }
                // 直接释放进程资源
                free_process(p); 
                release(&p->lock);
                release(&table_lock);
            
                // 返回进程pid
                return pid;
            }

            release(&p->lock);
        }
        
        // 当前没有正在运行的进程，无法睡眠
        if (!current_proc) {
            release(&table_lock);
            return -1;
        }

        // 没有可以回收的进程，睡眠等待
        sleep((void*)0xDEADBEEF, &table_lock);
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