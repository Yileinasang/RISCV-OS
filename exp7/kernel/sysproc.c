#include "def.h"

extern unsigned long sys_fork(void);
extern unsigned long sys_exit(int status);
extern unsigned long sys_wait(unsigned long addr);
extern unsigned long sys_getpid(void);

unsigned long sys_exit(int status) {
    exit_process(current_proc, status);
    return 0;
}

unsigned long sys_getpid(void) {
    return current_proc->pid;
}

unsigned long sys_fork(void) {
    return fork_process();
}

unsigned long sys_wait(unsigned long addr) {
    return wait_process(addr);
}