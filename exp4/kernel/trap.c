#include "riscv.h"
#include "def.h"

#define MAX_INTERRUPTS 128  // 最大支持的中断数

// 中断向量表
interrupt_handler_t interrupt_vector[MAX_INTERRUPTS];

void kernelvec();

void trap_init(void) {
    // 设置S模式陷阱向量，发生中断时跳转到kernelvec
    w_stvec((unsigned long)kernelvec);
    
    // 开启全局中断
    intr_on(); 

    // 初始化中断向量表
    for (int i = 0; i < MAX_INTERRUPTS; i++) {
        interrupt_vector[i] = 0;
    }
}

void kerneltrap(void) {
    // 保存中断发生时的CSR寄存器值
    unsigned long sepc = r_sepc();       
    unsigned long sstatus = r_sstatus(); 
    unsigned long scause = r_scause(); 

    // 检查中断来自S模式
    if ((sstatus & SSTATUS_SPP) == 0) {
        panic("kerneltrap: not from supervisor mode");
    }

    // 检查全局中断是否关闭，避免中断嵌套
    if (intr_get() != 0) {
        panic("kerneltrap: interrupts enabled");
    }

    // 如果是异常（最高位为0），调用异常处理
    if ((scause >> 63) == 0) {
        // printf("trap sepc: 0x%x\n", sepc >> 32);
        // printf("prev sepc: 0x%x\n", last_sepc);
        // if (sepc == 0xFFFFFFFF00000000UL) {
        //     sepc = last_sepc;
        //     printf("sepc set to last sepc: 0x%x\n", sepc);
        // }
        struct trapframe tf;
        tf.sepc = sepc;
        tf.sstatus = sstatus;
        tf.stval = r_stval();
        tf.scause = scause;

        // 调用异常处理函数
        handle_exception(&tf);

        // 如果修改了中断寄存器内容，需要写回
        w_sepc(tf.sepc);
        w_sstatus(tf.sstatus);
    } 
    else {
        // 调用中断分发函数
        interrupt_dispatch(scause);

        // 恢复中断前的CSR寄存器值
        w_sepc(sepc);
        w_sstatus(sstatus);
    }
}

void register_interrupt(int irq, interrupt_handler_t handler) {
    if (irq < 0 || irq >= MAX_INTERRUPTS) {
        panic("Invalid IRQ number");
    }

    // 将中断向量表中的处理函数设为handler
    interrupt_vector[irq] = handler;
}

void unregister_interrupt(int irq) {
    if (irq < 0 || irq >= MAX_INTERRUPTS) {
        panic("Invalid IRQ number");
    }

    // 清空中断向量表的处理函数
    interrupt_vector[irq] = 0;
}

void enable_interrupt(int irq) {
    if (irq < 0 || irq >= MAX_INTERRUPTS) {
        panic("Invalid IRQ number");
    }

    if (irq == IRQ_TIMER) {
        // 开启时钟中断
        w_sie(r_sie() | SIE_STIE); 
    } 
}

void disable_interrupt(int irq) {
    if (irq < 0 || irq >= MAX_INTERRUPTS) {
        panic("Invalid IRQ number");
    }

    if (irq == IRQ_TIMER){
        // 关闭时钟中断
        w_sie(r_sie() & ~SIE_STIE);  
    }
}

// 中断分发函数
void interrupt_dispatch(unsigned long scause) {
    if (scause == 0x8000000000000005L) {
        // 调用时钟中断处理函数
        if (interrupt_vector[IRQ_TIMER]) {
            interrupt_vector[IRQ_TIMER]();
        }
        else {
            panic("Unregistered timer interrupt\n");
        }
    } 
    else {
        panic("Unexpected interrupt\n");
    }
}

void dump_trapframe(struct trapframe *tf) {
    printf("=== Trapframe Dump ===\n");
    printf("sepc    : 0x%x\n", tf->sepc);
    printf("sstatus : 0x%x\n", tf->sstatus);
    printf("stval   : 0x%x\n", tf->stval);
    printf("scause  : 0x%x\n", tf->scause);
}

void handle_illegal_instruction(struct trapframe *tf) {
    printf("Exception: illegal instruction\n");
    tf->sepc += 4;
}

void handle_syscall(struct trapframe *tf) {
    printf("Exception: syscall\n");
    tf->sepc += 4;
}

int cnt = 0;
void handle_instruction_page_fault(struct trapframe *tf) {
    printf("Exception: instruction page fault\n");
    dump_trapframe(tf);
    tf->sepc += 4;
    cnt++;
    if (cnt >= 5) {
        panic("Too many instruction page faults");
    }
}

void handle_load_page_fault(struct trapframe *tf) {
    printf("Exception: load page fault\n");
    tf->sepc += 4;
}

void handle_store_page_fault(struct trapframe *tf) {
    printf("Exception: store page fault\n");
    tf->sepc += 4;
}

// 异常处理函数
void handle_exception(struct trapframe *tf) {
    // 去掉中断位
    unsigned long cause = r_scause() & (~(1UL << (8 * sizeof(unsigned long) - 1))); 
    printf("scause: 0x%x\n", cause);
    switch (cause) {
    case 2:
        // 非法指令
        handle_illegal_instruction(tf);
        break;
    case 9:  
        // 系统调用
        handle_syscall(tf);
        break;
    case 12: 
        // 指令页故障
        handle_instruction_page_fault(tf);
        break;
    case 13: 
        // 加载页故障
        handle_load_page_fault(tf);
        break;
    case 15: 
        // 存储页故障
        handle_store_page_fault(tf);
        break;
    default:
        panic("Unhandled exception");
    }
}