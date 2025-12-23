void sbi_set_timer(unsigned long time) {
    register unsigned long a0 asm("a0") = (unsigned long)time;
    register unsigned long a7 asm("a7") = 0UL;
    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
}
unsigned long get_time(void) { unsigned long t; asm volatile("rdtime %0" : "=r"(t)); return (unsigned long)t; }
