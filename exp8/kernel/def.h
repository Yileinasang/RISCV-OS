// global definitions
#define NULL ((void *)0)
#define PGSIZE 4096
#define UART0 0x10000000L
#define KERNBASE 0x80200000L
#define PHYSTOP 0x88000000L
#define IRQ_TIMER 127
#define IRQ_VIRTIO 1
#define NPROC 64
#define NAME_LEN 16
#define TRAMPOLINE (MAXVA - PGSIZE)
#define KSTACK(p) (TRAMPOLINE - ((p) + 1) * 2 * PGSIZE)
#define NOFILE 16
#define MAXPATH 128
#define O_RDONLY 0x000
#define O_WRONLY 0x001
#define O_RDWR 0x002
#define O_CREATE 0x200
#define O_TRUNC 0x400
#define T_DIR 1
#define T_FILE 2
#define T_DEVICE 3
#define DIRSIZ 14
#define NDEV 10
#define NFILE 100 
#define MAXARG 32
#define MAXOPBLOCKS 10 
#define LOGBLOCKS (MAXOPBLOCKS*3)
#define NBUF (MAXOPBLOCKS*3) 
#define FSMAGIC 0x10203040
#define BSIZE 1024
#define BPB (BSIZE*8)
#define NINODE 50
#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(unsigned int))
#define MAXFILE (NDIRECT + NINDIRECT)
#define ROOTDEV 1
#define ROOTINO 1
#define VIRTIO0 0x10001000
#define PLIC 0x0c000000L
#define PLIC_SENABLE ((volatile unsigned int *)(PLIC + 0x2080))
#define PLIC_SPRIORITY ((volatile unsigned int *)(PLIC + 0x201000))
#define PLIC_SCLAIM ((volatile unsigned int *)(PLIC + 0x201004))
#define FSSIZE 2000 
#define MAX_TIME 2

// uart.c
void uart_putc(char c);
void uart_puts(char *s);

// console.c
void console_putc(char c);
void console_puts(const char *s);

// printf.c
void print_number(int num, int base, int sign);
void print_ptr(unsigned long x);
int printf(const char *fmt, ...);
void panic(char*) __attribute__((noreturn));
void clear_screen(void);

// spinlock.c
struct spinlock {
    unsigned int locked;
    char *name;          
};
void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);

// kalloc.c
void pmm_init(void);
void freerange(void *pa_start, void *pa_end);
void *alloc_page(void);
void free_page(void *page);
void free_page_to_freelist(void *page);
void *alloc_pages(int n);

// string.c
void *memset(void *dst, int c, unsigned int n);
void *memmove(void *dst, const void *src, unsigned int n);
int strncmp(const char *p, const char *q, unsigned int n);
char *strncpy(char *s, const char *t, int n);
int strlen(const char *s);

// vm.c
typedef unsigned long pte_t;
typedef unsigned long* pagetable_t;
pagetable_t create_pagetable(void);
int map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long size, int perm);
void destroy_pagetable(pagetable_t pt);
pte_t* walk_create(pagetable_t pt, unsigned long va);
pte_t* walk_lookup(pagetable_t pt, unsigned long va);
void kvm_init(void);
void kvm_inithart(void);
void map_region(pagetable_t kpgtbl, unsigned long va, unsigned long pa, unsigned long sz, int perm);
void unmap_page(pagetable_t pagetable, unsigned long va, unsigned long npages, int do_free);
int copyout(pagetable_t pagetable, unsigned long dstva, const char *src, unsigned long len);
int copy_pagetable(pagetable_t old, pagetable_t new, unsigned long sz);

// trap.c
typedef void (*interrupt_handler_t)(void);
struct tframe {
    unsigned long sepc;
    unsigned long sstatus;
    unsigned long stval;
    unsigned long scause;
};
// extern unsigned long last_sepc;
void trap_init(void);
void kerneltrap(void);
void register_interrupt(int irq, interrupt_handler_t handler);
void unregister_interrupt(int irq);
void enable_interrupt(int irq);
void disable_interrupt(int irq);
void interrupt_dispatch(unsigned long scause);
void handle_syscall(struct tframe *tf);
void handle_instruction_page_fault(struct tframe *tf);
void handle_load_page_fault(struct tframe *tf);
void handle_store_page_fault(struct tframe *tf);
void handle_exception(struct tframe *tf);
void prepare_return(void);

// sbi.c
void sbi_set_timer(unsigned long time);
unsigned long get_time(void);

// proc.c
enum procstate {UNUSED, USED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE};
struct context {
    unsigned long ra;
    unsigned long sp;

    unsigned long s0;
    unsigned long s1;
    unsigned long s2;
    unsigned long s3;
    unsigned long s4;
    unsigned long s5;
    unsigned long s6;
    unsigned long s7;
    unsigned long s8;
    unsigned long s9;
    unsigned long s10;
    unsigned long s11;
};
struct trapframe {
  /*   0 */ unsigned long kernel_satp;
  /*   8 */ unsigned long kernel_sp;
  /*  16 */ unsigned long kernel_trap;
  /*  24 */ unsigned long epc;
  /*  32 */ unsigned long kernel_hartid;
  /*  40 */ unsigned long ra;
  /*  48 */ unsigned long sp;
  /*  56 */ unsigned long gp;
  /*  64 */ unsigned long tp;
  /*  72 */ unsigned long t0;
  /*  80 */ unsigned long t1;
  /*  88 */ unsigned long t2;
  /*  96 */ unsigned long s0;
  /* 104 */ unsigned long s1;
  /* 112 */ unsigned long a0;
  /* 120 */ unsigned long a1;
  /* 128 */ unsigned long a2;
  /* 136 */ unsigned long a3;
  /* 144 */ unsigned long a4;
  /* 152 */ unsigned long a5;
  /* 160 */ unsigned long a6;
  /* 168 */ unsigned long a7;
  /* 176 */ unsigned long s2;
  /* 184 */ unsigned long s3;
  /* 192 */ unsigned long s4;
  /* 200 */ unsigned long s5;
  /* 208 */ unsigned long s6;
  /* 216 */ unsigned long s7;
  /* 224 */ unsigned long s8;
  /* 232 */ unsigned long s9;
  /* 240 */ unsigned long s10;
  /* 248 */ unsigned long s11;
  /* 256 */ unsigned long t3;
  /* 264 */ unsigned long t4;
  /* 272 */ unsigned long t5;
  /* 280 */ unsigned long t6;
};
struct proc {
    struct spinlock lock;
    int pid;
    enum procstate state;
    void *kstack;
    struct context context;
    int xstate;
    int priority;
    void *chan;
    struct file *ofile[NOFILE];
    struct inode *cwd;
    struct proc *parent;
    unsigned long sz;
    pagetable_t pagetable;
    struct trapframe *trapframe;
    int timeslice;
    int timetotal;
};
extern struct proc proc_table[NPROC];
extern struct proc *current_proc;
void proc_init(void);
struct proc* alloc_process(void);
void free_process(struct proc *p);
int create_process(void (*entry)(void));
void exit_process(struct proc *p, int status);
int wait_process(unsigned long addr);
void set_proc_priority(int pid, int pri);
int get_proc_priority(int pid);
void scheduler_priority(void);
void scheduler_rotate(void);
void yield(void);
void sleep(void *chan, struct spinlock *lk);
void wakeup(void *chan);
int fork_process(void);
void forkret(void);
void scheduler_priority_extend(int aging);

// swtest.c
void producer_task(void);
void consumer_task(void);

// sleeplock.c
struct sleeplock {
    unsigned int locked;
    struct spinlock lk;
    char *name;
    int pid;
};
void initsleeplock(struct sleeplock *lk, char *name);
void acquiresleep(struct sleeplock *lk);
void releasesleep(struct sleeplock *lk);
int holdingsleep(struct sleeplock *lk);

// defs of file system
struct dirent {
    unsigned short inum;
    char name[DIRSIZ] __attribute__((nonstring));
};
struct inode {
    unsigned int dev;
    unsigned int inum;
    int ref;
    struct sleeplock lock;
    int valid;

    short type;
    short major;
    short minor;
    short nlink;
    unsigned int size;
    unsigned int addrs[NDIRECT + 1];
};
struct devsw {
    int (*read)(int, unsigned long, int);
    int (*write)(int, unsigned long, int);
};
struct file {
    enum {
        FD_NONE,
        FD_PIPE,
        FD_INODE,
        FD_DEVICE
    } type;
    int ref;
    char readable;
    char writable;
    struct pipe *pipe;
    struct inode *ip;
    unsigned int off;
    short major;
};
struct superblock {
    unsigned int magic; 
    unsigned int size;
    unsigned int nblocks; 
    unsigned int ninodes;
    unsigned int nlog;
    unsigned int logstart;
    unsigned int inodestart;
    unsigned int bmapstart;
};
struct buf {
    int valid;
    int disk; 
    unsigned int dev;
    unsigned int blockno;
    struct sleeplock lock;
    unsigned int refcnt;
    struct buf *prev;
    struct buf *next;
    unsigned char data[BSIZE];
};
struct dinode {
    short type;
    short major; 
    short minor;
    short nlink;
    unsigned int size; 
    unsigned int addrs[NDIRECT + 1]; 
};
#define IPB (BSIZE / sizeof(struct dinode))
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)
#define BBLOCK(b, sb) ((b) / BPB + sb.bmapstart)
extern struct superblock sb;

// sysfile.c
int sys_read(int fd, char *buf, int n);
int sys_write(int fd, char *buf, int n);
int sys_close(int fd);
int sys_unlink(char *path);
int sys_open(char *path, int omode);
struct inode *create(char *path, short type, short major, short minor);

// file.c
void fileinit(void);
struct file *filealloc(void);
struct file *filedup(struct file *f);
void fileclose(struct file *f);
int fileread(struct file *f, unsigned long addr, int n);
int filewrite(struct file *f, unsigned long addr, int n);

// fs.c
void fsinit(int dev);
void iinit();
struct inode *ialloc(unsigned int dev, short type);
void iupdate(struct inode *ip);
struct inode *idup(struct inode *ip);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iput(struct inode *ip);
void iunlockput(struct inode *ip);
void ireclaim(int dev);
void itrunc(struct inode *ip);
int readi(struct inode *ip, int user_dst, unsigned long dst, unsigned int off, unsigned int n);
int writei(struct inode *ip, int user_src, unsigned long src, unsigned int off, unsigned int n);
int namecmp(const char *s, const char *t);
struct inode *dirlookup(struct inode *dp, char *name, unsigned int *poff);
int dirlink(struct inode *dp, char *name, unsigned int inum);
struct inode *namei(char *path);
struct inode *nameiparent(char *path, char *name);
void readsb(int dev, struct superblock *sb);

// virtio_disk.c
void virtio_disk_init(void);
void virtio_disk_rw(struct buf *b, int write);
void virtio_disk_intr(void);

// bio.c
void binit(void);
struct buf *bread(unsigned int dev, unsigned int blockno);
void bwrite(struct buf *b);
void brelse(struct buf *b);
void bpin(struct buf *b);
void bunpin(struct buf *b);
void bcache_reset(void);

// log.c
void initlog(int dev, struct superblock *sb);
void begin_op(void);
void end_op(void);
void log_write(struct buf *b);

// snprintf.c
int snprintf(char *str, unsigned int size, const char *format, ...);

// sysproc.c
unsigned long sys_exit(int status);
unsigned long sys_getpid(void);
unsigned long sys_fork(void);
unsigned long sys_wait(unsigned long addr);