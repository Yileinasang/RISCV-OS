#include "def.h"
#include "assert.h"
#include "riscv.h"

__attribute__ ((aligned (16))) char stack_top[4096];

// unsigned long last_sepc = 0x80200000;
struct proc proc_table[NPROC];
struct proc *current_proc = 0;


// Lab4
volatile int interrupt_count = 0;
void timer_interrupt(void) {
    interrupt_count++;
    printf("Timer interrupt triggered! Count: %d\n", interrupt_count);

    // 设置下一次时钟中断
    sbi_set_timer(get_time() + 1000000);
}
void test_timer_interrupt(void) { 
    printf("Testing timer interrupt...\n"); 

    // 注册时钟中断处理函数
    register_interrupt(IRQ_TIMER, timer_interrupt);

    // 先安排第一次时钟中断
    unsigned long start_time = get_time();
    sbi_set_timer(start_time + 1000000);  // 安排首次触发

    // 开启时钟中断
    enable_interrupt(IRQ_TIMER);

    // 等待5次中断
    while (interrupt_count < 5) { 
        printf("Waiting for interrupt %d...\n", interrupt_count + 1); 
        // 简单延时
        for (volatile int i = 0; i < 10000000; i++);
    } 
 
    // 记录中断后的时间
    unsigned long end_time = get_time();

    // 打印测试结果
    printf("Timer test completed:\n");
    printf("Start time: %x\n", start_time);
    printf("End time: %x\n", end_time);
    printf("Total interrupts: %d\n", interrupt_count);

    // 注销时钟中断处理函数
    unregister_interrupt(IRQ_TIMER);

    // 关闭时钟中断
    disable_interrupt(IRQ_TIMER);
}
void test_exception_handling(void) {
    printf("Testing exception handling...\n");

    // 非法指令异常
    printf("Illegal Instruction Test\n");
    asm volatile(".word 0xFFFFFFFF\n"
                 "nop\n"              
                 "nop\n"
                 "nop\n");

    // 系统调用异常（需要做用户态和内核态切换）
    // printf("System Call Exception\n");
    // asm volatile("ecall\n");
    // asm volatile("nop\nnop\nnop\n");

    // 指令页异常（越界地址跳转有问题）
    // printf("Instruction Page Fault\n");
    // volatile unsigned long *invalid_instr = (unsigned long *)0xFFFFFFFF00000000UL;
    // printf ("Jumping to invalid instruction address: 0x%x\n", (unsigned long)invalid_instr>>32);
    // last_sepc = r_sepc();
    // printf("Current sepc: 0x%x\n", last_sepc);
    // asm volatile(
    //     "mv t0, %0\n\t"
    //     "jalr x0, 0(t0)\n\t"
    //     :
    //     : "r"(invalid_instr)
    //     : "t0", "memory"
    // );
    // asm volatile("nop\nnop\nnop\n");

    // 加载页异常
    printf("Load Page Fault Test\n");
    volatile unsigned long *bad_load = (unsigned long *)0xFFFFFFFF00000000UL;
    unsigned long bad_value = *bad_load;
    (void)bad_value;
    asm volatile("nop\nnop\nnop\n");

    // 存储页异常
    printf("Store Page Fault Test\n");
    volatile unsigned long *bad_store = (unsigned long *)0xFFFFFFFF00000000UL;
    *bad_store = 0x66;
    asm volatile("nop\nnop\nnop\n");

    printf("Exception tests completed\n");
}
void pt_init(void) {
    pmm_init();
    kvm_init();
    kvm_inithart();
}



void main() {
    pt_init();
    trap_init();
    test_timer_interrupt();
    test_exception_handling();
}