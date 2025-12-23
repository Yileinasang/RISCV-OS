#include "riscv.h"
#include "def.h"

struct logheader {
    int n;
    int block[LOGBLOCKS];
};

struct log {
    struct spinlock lock;
    int start;
    int outstanding;
    int committing;
    int dev;
    struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void initlog(int dev, struct superblock *sb)
{
    if (sizeof(struct logheader) >= BSIZE)
        panic("initlog: too big logheader");

    initlock(&log.lock, "log");
    log.start = sb->logstart;
    log.dev = dev;
    recover_from_log();
}

static void install_trans(int recovering) {
    int tail;

    for (tail = 0; tail < log.lh.n; tail++) {
        if (recovering) {
            printf("recovering tail %d dst %d\n", tail, log.lh.block[tail]);
        }
        
        // 读取日志块
        struct buf *lbuf = bread(log.dev, log.start + tail + 1);
        // 读取目标块
        struct buf *dbuf = bread(log.dev, log.lh.block[tail]);
        
        // 将日志块内容写入目标块
        memmove(dbuf->data, lbuf->data, BSIZE);
        // 将目标块内容写入磁盘
        bwrite(dbuf);
        
        // 不是恢复操作，解除目标块固定
        if (recovering == 0)
            bunpin(dbuf);
        
        brelse(lbuf);
        brelse(dbuf);
    }
}

static void read_head(void) {
    // 从磁盘中读取日志头
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *lh = (struct logheader *)(buf->data);
    int i;
    // 读取事务涉及的块数
    log.lh.n = lh->n;
    for (i = 0; i < log.lh.n; i++) {
        // 读取事务涉及的块号
        log.lh.block[i] = lh->block[i];
    }
    brelse(buf);
}

static void write_head(void) {
    struct buf *buf = bread(log.dev, log.start);
    struct logheader *hb = (struct logheader *)(buf->data);
    int i;
    hb->n = log.lh.n;
    for (i = 0; i < log.lh.n; i++) {
        hb->block[i] = log.lh.block[i];
    }
    bwrite(buf);
    brelse(buf);
}

static void recover_from_log(void) {
    read_head();
    // 恢复日志中的数据块到目标位置
    install_trans(1);
    // 清空日志头
    log.lh.n = 0;
    write_head();
}

void begin_op(void) {
    acquire(&log.lock);
    while (1) {
        // 等待当前提交完成
        if (log.committing) {
            sleep(&log, &log.lock);
        }
        // 等待日志空间足够
        else if (log.lh.n + (log.outstanding + 1) * MAXOPBLOCKS > LOGBLOCKS) {
            sleep(&log, &log.lock);
        }
        else {
            // 事务计数增加，开始文件操作
            log.outstanding += 1;
            release(&log.lock);
            break;
        }
    }
}

void end_op(void) {
    int do_commit = 0;

    acquire(&log.lock);
    // 事务计数减少，结束文件操作
    log.outstanding -= 1;
    // 结束操作时不应该处于提交状态
    if (log.committing)
        panic("log.committing");

    if (log.outstanding == 0) {
        // 最后一个事务，准备提交
        do_commit = 1;
        log.committing = 1;
    }
    else {
        wakeup(&log);
    }
    release(&log.lock);

    if (do_commit) {
        // 提交日志
        commit();
        acquire(&log.lock);
        log.committing = 0;
        wakeup(&log);
        release(&log.lock);
    }
}

static void write_log(void) {
    int tail;

    for (tail = 0; tail < log.lh.n; tail++) {
        struct buf *to = bread(log.dev, log.start + tail + 1); 
        struct buf *from = bread(log.dev, log.lh.block[tail]); 
        memmove(to->data, from->data, BSIZE);
        bwrite(to); // write the log
        brelse(from);
        brelse(to);
    }
}

static void commit() {
    // 日志中存在需要提交的事务
    if (log.lh.n > 0) {
        write_log(); 
        write_head();
        // 将日志中的数据块写回目标位置
        install_trans(0);
        log.lh.n = 0;
        write_head();
    }
}

void log_write(struct buf *b) {
    int i;

    acquire(&log.lock);
    if (log.lh.n >= LOGBLOCKS)
        panic("too big a transaction");
    if (log.outstanding < 1)
        panic("log_write outside of trans");

    for (i = 0; i < log.lh.n; i++) {
        if (log.lh.block[i] == b->blockno)
            // 缓冲区已经在日志中，直接退出
            break;
    }
    // 将缓冲区块号添加到日志头
    log.lh.block[i] = b->blockno;

    // 如果缓冲区不在日志中，固定缓冲区并增加事务块数
    if (i == log.lh.n) { 
        bpin(b);
        log.lh.n++;
    }
    release(&log.lock);
}
