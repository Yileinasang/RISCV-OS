#include "riscv.h"
#include "def.h"

struct devsw devsw[NDEV];
struct {
    struct spinlock lock;
    struct file file[NFILE];
} ftable;

void fileinit(void) {
    initlock(&ftable.lock, "ftable");
}

struct file *filealloc(void) {
    struct file *f;

    acquire(&ftable.lock);
    for (f = ftable.file; f < ftable.file + NFILE; f++) {
        if (f->ref == 0) {
            // 找到一个空闲的文件，初始化引用计数
            f->ref = 1;
            release(&ftable.lock);
            return f;
        }
    }
    release(&ftable.lock);
    return 0;
}

struct file *filedup(struct file *f) {
    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("filedup");
    // 增加文件的引用计数
    f->ref++;
    release(&ftable.lock);
    return f;
}

void fileclose(struct file *f) {
    struct file ff;

    acquire(&ftable.lock);
    if (f->ref < 1)
        panic("fileclose");
    if (--f->ref > 0) {
        // 如果减少引用计数后仍有引用，直接返回
        release(&ftable.lock);
        return;
    }

    // 清空文件信息
    ff = *f;
    f->ref = 0;
    f->type = FD_NONE;
    release(&ftable.lock);
    
    if (ff.type == FD_INODE) {
        // 如果文件是inode类型，释放对应的inode
        begin_op();
        iput(ff.ip);
        end_op();
    }
}

int fileread(struct file *f, unsigned long addr, int n) {
    int r = 0;

    if (f->readable == 0)
        return -1;
    
    if (f->type == FD_INODE) {
        ilock(f->ip);
        if ((r = readi(f->ip, 0, addr, f->off, n)) > 0)
            // 读取完成，更新偏移量
            f->off += r;
        iunlock(f->ip);
    }
    else
        panic("fileread");

    return r;
}

int filewrite(struct file *f, unsigned long addr, int n) {
    int r, ret = 0;

    if (f->writable == 0)
        return -1;

    if (f->type == FD_INODE) {
        // 设定最大写入字节数
        int max = ((MAXOPBLOCKS - 1 - 1 - 2) / 2) * BSIZE;
        int i = 0;
        while (i < n) {
            int n1 = n - i;
            if (n1 > max)
                n1 = max;

            begin_op();
            ilock(f->ip);
            if ((r = writei(f->ip, 0, addr + i, f->off, n1)) > 0)
                // 写入完成，更新偏移量
                f->off += r;
            iunlock(f->ip);
            end_op();

            // 成功写入的字节数不等于要求的字节数，出现错误
            if (r != n1)
                break;

            // 更新已写入的字节数
            i += r;
        }

        // 写入的字节数等于总字节数，返回总字节数，否则报错
        ret = (i == n ? n : -1);
    }
    else
        // inode不是文件，不能写入
        panic("filewrite");

    return ret;
}
