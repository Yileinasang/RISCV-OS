#include "def.h"
#include "assert.h"
#include "riscv.h"
#include "syscall.h"

__attribute__ ((aligned (16))) char stack_top[16384];

// unsigned long last_sepc = 0x80200000;
struct proc proc_table[NPROC];
struct proc *current_proc = 0;
struct superblock sb;

void pt_init(void) {
    pmm_init();
    kvm_init();
    kvm_inithart();
}

void cpu_task_high(void) {
    volatile unsigned long sum = 0;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 1000000; j++)
            sum += j;
        printf("HIGH iter %d\n", i);
        // 让出CPU使得调度器运行其他进程
        yield();
    }
    printf("HIGH task completed\n");
    
    // 终止当前进程
    exit_process(current_proc, 0);
}
void cpu_task_med(void) {
    volatile unsigned long sum = 0;
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 500000; j++)
            sum += j;
        printf("MED  iter %d\n", i);
        yield();
    }
    printf("MED  task completed\n");

    exit_process(current_proc, 0);
}
// Lab8
void test_scheduler_1(void) {
    printf("Testing scheduler 1...\n");

    int pid_high = create_process(cpu_task_high);
    int pid_med = create_process(cpu_task_med);

    if (pid_high <= 0 || pid_med <= 0 ) {
        printf("create_process failed: %d %d\n", pid_high, pid_med);
        return;
    }

    // 设置高低优先级
    set_proc_priority(pid_high, 50);
    set_proc_priority(pid_med, 10);
    printf("Set process priorities, HIGH = %d, MED = %d\n", get_proc_priority(pid_high), get_proc_priority(pid_med));

    // 启动调度器
    scheduler_priority_extend(0);
    printf("Scheduler test 1 completed\n");
}
void test_scheduler_2(void) {
    printf("Testing scheduler 2...\n");

    int pid_high = create_process(cpu_task_high);
    int pid_med = create_process(cpu_task_med);

    if (pid_high <= 0 || pid_med <= 0 ) {
        printf("create_process failed: %d %d\n", pid_high, pid_med);
        return;
    }

    // 设置相等优先级
    set_proc_priority(pid_high, 50);
    set_proc_priority(pid_med, 50);
    printf("Set process priorities, HIGH = %d, MED = %d\n", get_proc_priority(pid_high), get_proc_priority(pid_med));

    // 启动调度器
    scheduler_priority_extend(0);
    printf("Scheduler test 2 completed\n");
}
void test_scheduler_3(void) {
    printf("Testing scheduler 3...\n");

    int pid_high = create_process(cpu_task_high);
    int pid_med = create_process(cpu_task_med);

    if (pid_high <= 0 || pid_med <= 0 ) {
        printf("create_process failed: %d %d\n", pid_high, pid_med);
        return;
    }

    // 设置近似优先级
    set_proc_priority(pid_high, 50);
    set_proc_priority(pid_med, 49);
    printf("Set process priorities, HIGH = %d, MED = %d\n", get_proc_priority(pid_high), get_proc_priority(pid_med));

    // 启动调度器，启用老化机制
    scheduler_priority_extend(1);
    printf("Scheduler test 3 completed\n");
}

void main() {
        pt_init();
        proc_init();
        test_scheduler_1();
        test_scheduler_2();
        test_scheduler_3();
}