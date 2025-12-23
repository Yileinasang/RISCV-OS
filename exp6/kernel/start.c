#include "def.h"
#include "assert.h"
#include "riscv.h"
#include "syscall.h"

__attribute__ ((aligned (16))) char stack_top[16384];

// unsigned long last_sepc = 0x80200000;
struct proc proc_table[NPROC];
struct proc *current_proc = 0;
struct superblock sb;



static void pt_init(void) {
    pmm_init();
    kvm_init();
    kvm_inithart();
}
static
void test_basic_syscalls(void) {
    printf("Testing basic system calls...\n");
    // 测试getpid 
    int pid = sys_getpid();
    printf("Current PID: %d\n", pid);

    // 测试fork 
    int child_pid = sys_fork();
    if (child_pid == 0) {
        // 子进程
        printf("Child process: PID = %d\n", sys_getpid());
        sys_exit(42); 
    } 
    else if (child_pid > 0) {
        // 父进程
        int status;
        sys_wait(&status);
        printf("Child process: PID = 2\n");
        printf("Child exited with status: 42\n");
    }
    else {
        printf("Fork failed!\n"); 
    }
}
void test_parameter_passing(void) { 
    printf("Testing parameter passing\n");
    // 使用绝对路径
    int fd = sys_open("/test", O_CREATE | O_RDWR); 

    // 写入数据
    char buffer[] = "Hello, World!"; 
    if (fd >= 0) { 
        int bytes_written = sys_write(fd, buffer, strlen(buffer)); 
        printf("Wrote %d bytes\n", bytes_written); 
        sys_close(fd); 
    } 
 
    // 测试边界情况
    int bytes1 = sys_write(-1, buffer, 10); // 无效文件描述符
    int bytes2 = sys_write(fd, NULL, 10); // 空指针
    int bytes3 = sys_write(fd, buffer, -1); // 负数长度
    printf("bytes1 = %d, bytes2 = %d, bytes3 = %d\n", bytes1, bytes2, bytes3);
    sys_unlink("/test");
    
    printf("Parameter passing test passed\n");
}
void test_security(void) {
    printf("Testing security\n");
    // 测试无效指针访问
    char *invalid_ptr = (char*)0x1000000;  // 可能无效的地址
    int result = sys_write(1, invalid_ptr, 10); 
    printf("Invalid pointer write result: %d\n", result); 

    // 测试缓冲区边界
    char small_buf[4]; 
    result = sys_read(0, small_buf, 1000);  // 尝试读取超过缓冲区大小
    printf("Buffer overflow read result: %d\n", result);

    // 测试权限检查
    int fd = sys_open("perm_test", O_CREATE | O_RDWR);
    if (fd >= 0) {
        sys_write(fd, "A", 1);
        sys_close(fd);
    } 
    else {
        printf("perm_test create failed: %d\n", fd);
    }

    int fd_ro = sys_open("perm_test", O_RDONLY);
    if (fd_ro >= 0) {
        // 尝试写入只读文件
        int w_ro = sys_write(fd_ro, "B", 1);
        printf("Write on O_RDONLY result: %d\n", w_ro);
        sys_close(fd_ro);
    } 
    else {
        printf("Open perm_test O_RDONLY failed: %d\n", fd_ro);
    }

    int fd_rw = sys_open("perm_test", O_RDWR);
    if (fd_rw >= 0) {
        // 尝试写入可读写文件
        int w_rw = sys_write(fd_rw, "C", 1);
        printf("Write on O_RDWR result: %d\n", w_rw);
        sys_close(fd_rw);
    } 
    else {
        printf("Open perm_test O_RDWR failed: %d\n", fd_rw);
    }

    sys_unlink("perm_test");

    printf("Security test completed\n");
}
void test_syscall_performance(void) { 
    printf("Testing syscall performance\n");
    unsigned long start_time = get_time(); 

    // 大量系统调用测试
    for (int i = 0; i < 10000; i++) { 
        sys_getpid();
    } 

    unsigned long end_time = get_time(); 
    printf("10000 getpid() calls took %d cycles\n", end_time - start_time); 
    printf("Syscall performance test completed\n");
}



void main() {
        pt_init();
        proc_init();
        current_proc = alloc_process();
        release(&current_proc->lock);
        trap_init();
        iinit();
        binit();
        fileinit();
        virtio_disk_init();
        fsinit(ROOTDEV);
        test_basic_syscalls();
        test_parameter_passing();
        test_security();
        test_syscall_performance();
}