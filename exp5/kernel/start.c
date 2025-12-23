#include "def.h"
#include "assert.h"
#include "riscv.h"

__attribute__ ((aligned (16))) char stack_top[4096];

struct proc proc_table[NPROC];
struct proc *current_proc = 0;

void pt_init(void) {
    pmm_init();
    kvm_init();
    kvm_inithart();
}

// Lab5
void simple_task(void) { int a = 1; (void)a; }

void test_process_creation(void) {
    printf("Testing process creation...\n");
    int pid = create_process(simple_task);
    assert(pid > 0);
    int pids[NPROC];
    int count = 1;
    for (int i = 0; i < NPROC + 5; i++) {
        int pid2 = create_process(simple_task);
        if (pid2 > 0) pids[count++] = pid2; else break;
    }
    printf("Created %d processes\n", count);
    for (int i = 0; i < count; i++) { wait_process(NULL); }
    printf("Process creation test completed\n");
}

void cpu_task_high(void) {
    volatile unsigned long sum = 0;
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 1000000; j++) sum += j;
        printf("HIGH iter %d\n", i);
        yield();
    }
    printf("HIGH task completed\n");
    exit_process(current_proc, 0);
}
void cpu_task_med(void) {
    volatile unsigned long sum = 0;
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 500000; j++) sum += j;
        printf("MED  iter %d\n", i);
        yield();
    }
    printf("MED  task completed\n");
    exit_process(current_proc, 0);
}
void cpu_task_low(void) {
    volatile unsigned long sum = 0;
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 100000; j++) sum += j;
        printf("LOW  iter %d\n", i);
        yield();
    }
    printf("LOW  task completed\n");
    exit_process(current_proc, 0);
}

void test_scheduler(void) {
    printf("Testing scheduler...\n");
    int pid_high = create_process(cpu_task_high);
    int pid_med  = create_process(cpu_task_med);
    int pid_low  = create_process(cpu_task_low);
    if (pid_high <= 0 || pid_med <= 0 || pid_low <= 0) {
        printf("create_process failed: %d %d %d\n", pid_high, pid_med, pid_low);
        return;
    }
    printf("Created processes: HIGH = %d, MED = %d, LOW = %d\n", pid_high, pid_med, pid_low);
    set_proc_priority(pid_high, 50);
    set_proc_priority(pid_med, 49);
    set_proc_priority(pid_low, 48);
    printf("Set process priorities\n");
    scheduler_priority();
    printf("Scheduler test completed\n");
}

// swtest
void shared_buffer_init(void);
void producer_task(void);
void consumer_task(void);

void test_synchronization(void) {
    printf("Starting synchronization test\n");
    shared_buffer_init();
    int pid_p = create_process(producer_task);
    int pid_c = create_process(consumer_task);
    if (pid_p <= 0 || pid_c <= 0) {
        printf("create_process failed: %d %d\n", pid_p, pid_c);
        return;
    }
    scheduler_rotate();
    printf("Synchronization test completed\n");
}

void main() {
    pt_init();
    proc_init();
    test_process_creation();
    test_scheduler();
    test_synchronization();
}
