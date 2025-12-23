#include "riscv.h"
#include "def.h"

#define MAX_INTERRUPTS 128
interrupt_handler_t interrupt_vector[MAX_INTERRUPTS];
void kernelvec();

void trap_init(void) {
    w_stvec((unsigned long)kernelvec);
    intr_on();
    for (int i = 0; i < MAX_INTERRUPTS; i++) interrupt_vector[i] = 0;
}

void kerneltrap(void) {
    unsigned long sepc = r_sepc();       
    unsigned long sstatus = r_sstatus(); 
    unsigned long scause = r_scause(); 
    if ((sstatus & SSTATUS_SPP) == 0) panic("kerneltrap: not from supervisor mode");
    if (intr_get() != 0) panic("kerneltrap: interrupts enabled");
    if ((scause >> 63) == 0) {
        struct trapframe tf; tf.sepc = sepc; tf.sstatus = sstatus; tf.stval = r_stval(); tf.scause = scause;
        handle_exception(&tf);
        w_sepc(tf.sepc); w_sstatus(tf.sstatus);
    } else {
        interrupt_dispatch(scause);
        w_sepc(sepc); w_sstatus(sstatus);
    }
}

void register_interrupt(int irq, interrupt_handler_t handler) { if (irq < 0 || irq >= MAX_INTERRUPTS) panic("Invalid IRQ number"); interrupt_vector[irq] = handler; }
void unregister_interrupt(int irq) { if (irq < 0 || irq >= MAX_INTERRUPTS) panic("Invalid IRQ number"); interrupt_vector[irq] = 0; }
void enable_interrupt(int irq) { if (irq < 0 || irq >= MAX_INTERRUPTS) panic("Invalid IRQ number"); if (irq == IRQ_TIMER) { w_sie(r_sie() | SIE_STIE); } }
void disable_interrupt(int irq) { if (irq < 0 || irq >= MAX_INTERRUPTS) panic("Invalid IRQ number"); if (irq == IRQ_TIMER){ w_sie(r_sie() & ~SIE_STIE); } }

void interrupt_dispatch(unsigned long scause) {
    if (scause == 0x8000000000000005L) {
        if (interrupt_vector[IRQ_TIMER]) { interrupt_vector[IRQ_TIMER](); }
        else { panic("Unregistered timer interrupt\n"); }
    } else { panic("Unexpected interrupt\n"); }
}

void dump_trapframe(struct trapframe *tf) {
    printf("=== Trapframe Dump ===\n");
    printf("sepc    : 0x%x\n", tf->sepc);
    printf("sstatus : 0x%x\n", tf->sstatus);
    printf("stval   : 0x%x\n", tf->stval);
    printf("scause  : 0x%x\n", tf->scause);
}

void handle_illegal_instruction(struct trapframe *tf) { printf("Exception: illegal instruction\n"); tf->sepc += 4; }
void handle_syscall(struct trapframe *tf) { printf("Exception: syscall\n"); tf->sepc += 4; }
void handle_instruction_page_fault(struct trapframe *tf) { printf("Exception: instruction page fault\n"); dump_trapframe(tf); tf->sepc += 4; }
void handle_load_page_fault(struct trapframe *tf) { printf("Exception: load page fault\n"); tf->sepc += 4; }
void handle_store_page_fault(struct trapframe *tf) { printf("Exception: store page fault\n"); tf->sepc += 4; }

void handle_exception(struct trapframe *tf) {
    unsigned long cause = r_scause() & (~(1UL << (8 * sizeof(unsigned long) - 1))); 
    printf("scause: 0x%x\n", cause);
    switch (cause) {
    case 2: handle_illegal_instruction(tf); break;
    case 9: handle_syscall(tf); break;
    case 12: handle_instruction_page_fault(tf); break;
    case 13: handle_load_page_fault(tf); break;
    case 15: handle_store_page_fault(tf); break;
    default: panic("Unhandled exception");
    }
}
