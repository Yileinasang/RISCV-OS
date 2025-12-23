// OpenSBI 兼容的 legacy 调用：a7=0 (SBI_SET_TIMER)，a0=stime value
void sbi_set_timer(unsigned long time) {
    register unsigned long a0 asm("a0") = (unsigned long)time;
    register unsigned long a7 asm("a7") = 0UL;
    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a7)
                 : "memory");
}

// 读取 time CSR
unsigned long get_time(void) {
    unsigned long t;
    asm volatile("rdtime %0" : "=r"(t));
    return (unsigned long)t;
}