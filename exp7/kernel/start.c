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


void test_filesystem_integrity(void) { 
    printf("Testing filesystem integrity\n"); 
    // 使用相对路径
    int fd = sys_open("testfile", O_CREATE | O_RDWR); 
    assert(fd >= 0); 
    
    // 写入数据
    char buffer[] = "Hello, filesystem!"; 
    int bytes = sys_write(fd, buffer, strlen(buffer)); 
    printf("wrote %d bytes\n", bytes);
    assert(bytes == strlen(buffer)); 
    sys_close(fd); 
    
    // 重新打开并验证数据相同
    fd = sys_open("testfile", O_RDONLY); 
    assert(fd >= 0); 
    char read_buffer[64]; 
    bytes = sys_read(fd, read_buffer, sizeof(read_buffer)); 
    read_buffer[bytes] = '\0'; 
    assert(strncmp(buffer, read_buffer, bytes) == 0); 
    sys_close(fd); 
    
    // 删除文件
    assert(sys_unlink("testfile") == 0); 

    printf("Filesystem integrity test passed\n"); 
}
void concurrent_file_access_task(void) {
    char filename[32];
    int pid = current_proc->pid;
    // 根据pid生成唯一文件名
    snprintf(filename, sizeof(filename), "testfile_%d", pid);  

    // 创建文件
    int fd = sys_open(filename, O_CREATE | O_RDWR);
    if (fd < 0) {
        printf("Process %d: Failed to create file %s\n", pid, filename);
        return;
    }
    printf("Process %d: Created file %s\n", pid, filename);

    // 写入数据
    char buffer[] = "Concurrent access test!";
    int bytes_written = sys_write(fd, buffer, strlen(buffer));
    if (bytes_written != strlen(buffer)) {
        printf("Process %d: Failed to write to file %s\n", pid, filename);
        sys_close(fd);
        return;
    }
    printf("Process %d: Wrote %d bytes to file %s\n", pid, bytes_written, filename);
    sys_close(fd);
    yield();

    // 重新打开文件并读取数据
    fd = sys_open(filename, O_RDONLY);
    if (fd < 0) {
        printf("Process %d: Failed to reopen file %s\n", pid, filename);
        return;
    }
    char read_buffer[64];
    int bytes_read = sys_read(fd, read_buffer, sizeof(read_buffer) - 1);
    if (bytes_read < 0) {
        printf("Process %d: Failed to read from file %s\n", pid, filename);
        sys_close(fd);
        return;
    }
    read_buffer[bytes_read] = '\0';  // 确保字符串以 '\0' 结尾
    printf("Process %d: Read %d bytes from file %s\n", pid, bytes_read, filename);
    sys_close(fd);

    // 删除文件
    if (sys_unlink(filename) == 0)
        printf("Process %d: Deleted file %s\n", pid, filename);
    else
        printf("Process %d: Failed to delete file %s\n", pid, filename);
    
    exit_process(current_proc, 0);
}
void test_concurrent_file_access(void) {
    printf("Testing concurrent file access\n");

    int num_processes = 5;
    int pids[5];

    for (int i = 0; i < num_processes; i++) {
        int pid = create_process(concurrent_file_access_task);
        if (pid > 0) {
            pids[i] = pid;
        } 
        else {
            printf("Failed to create process %d\n", i);
        }
    }

    // 使用轮转调度器
    scheduler_rotate();

    printf("Concurrent file access test completed\n");
}
void test_filesystem_performance(void) {
    printf("Testing filesystem performance\n"); 

    // 小文件测试
    unsigned long start_time = get_time();
    for (int i = 0; i < 1000; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "small_%d", i);
        int fd = sys_open(filename, O_CREATE | O_RDWR);
        sys_write(fd, "test", 4); 
        sys_close(fd); 
    }
    unsigned long small_files_time = get_time() - start_time;
    printf("Small files (1000x4B): %d cycles\n", small_files_time);

    // 大文件测试，由于bmap只支持一个间接块，128KB才小于上限
    start_time = get_time();
    int fd = sys_open("large_file", O_CREATE | O_RDWR);
    char large_buffer[4096];
    for (int i = 0; i < 32; i++) {
        sys_write(fd, large_buffer, sizeof(large_buffer));
    }
    sys_close(fd);
    unsigned long large_file_time = get_time() - start_time;
    printf("Large file (1x128KB): %d cycles\n", large_file_time);

    // 清理测试文件
    for (int i = 0; i < 1000; i++) {
        char filename[32];
        snprintf(filename, sizeof(filename), "small_%d", i);
        sys_unlink(filename);
    }

    sys_unlink("large_file");
    printf("Filesystem performance test completed\n");
}
static void simulate_crash_reboot(void) {
    // 清除缓冲区模拟崩溃
    bcache_reset();
    // 重启文件系统
    readsb(ROOTDEV, &sb);
    initlog(ROOTDEV, &sb);
}
static void unsafe_detach_fd(int fd) {
    // 进行不安全的文件关闭
    if (fd < 0) return;
    struct file *f = current_proc->ofile[fd];
    if (!f) return;
    current_proc->ofile[fd] = 0;
    f->ref--;
}
static void unsafe_uncommitted_overwrite(const char *name, const char *data) {
    // 只开始日志操作，但不结束，会出现崩溃
    begin_op();
    int fd = sys_open((char*)name, O_RDWR);
    struct file *f = current_proc->ofile[fd];
    ilock(f->ip);
    writei(f->ip, 0, (unsigned long)data, 0, 1);
    iunlock(f->ip);
    unsafe_detach_fd(fd);
}
void test_crash_recovery(void) {
    printf("Testing crash recovery with log\n");
    char buf[4] = {0};

    // 正常写入A
    int fd = sys_open("logtest", O_CREATE | O_TRUNC | O_RDWR);
    sys_write(fd, "A", 1);
    sys_close(fd);

    // 重启验证A成功提交
    simulate_crash_reboot();
    fd = sys_open("logtest", O_RDONLY);
    sys_read(fd, buf, 1);
    sys_close(fd);
    printf("After reboot committed A -> %c\n", buf[0]);

    // 正常写入B
    fd = sys_open("logtest", O_RDWR);
    sys_write(fd, "B", 1);
    sys_close(fd);

    // 不安全写入C，应该不能提交，文件内容仍为B
    unsafe_uncommitted_overwrite("logtest", "C");

    // 重启验证C未提交
    simulate_crash_reboot();
    memset(buf, 0, sizeof(buf));
    fd = sys_open("logtest", O_RDONLY);
    sys_read(fd, buf, 1);
    sys_close(fd);
    printf("After uncommitted overwrite crash expect B -> %c\n", buf[0]);

    sys_unlink("logtest");

    printf("Crash recovery test finished\n");
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
        test_filesystem_integrity();
        test_concurrent_file_access();
        current_proc = alloc_process();
        release(&current_proc->lock);
        test_filesystem_performance();
        test_crash_recovery();
}