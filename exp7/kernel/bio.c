#include "riscv.h"
#include "def.h"

struct {
    struct spinlock lock;
    struct buf buf[NBUF];
    struct buf head;
} bcache;

void binit(void) {
    struct buf *b;

    initlock(&bcache.lock, "bcache");

    // 初始化双向链表
    bcache.head.prev = &bcache.head;
    bcache.head.next = &bcache.head;

    // 初始化缓冲区
    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        initsleeplock(&b->lock, "buffer");
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }
}

static struct buf *bget(unsigned int dev, unsigned int blockno) {
    struct buf *b;

    // 获取缓存的锁
    acquire(&bcache.lock);

    // 检查当前块是否已在缓存中
    for (b = bcache.head.next; b != &bcache.head; b = b->next) {
        if (b->dev == dev && b->blockno == blockno) {
            // 增加引用计数
            b->refcnt++;
            
            // 释放缓存锁，才能获取睡眠锁
            release(&bcache.lock);
            
            // 获取缓冲区的睡眠锁
            acquiresleep(&b->lock);
            return b;
        }
    }

    for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
        // 找到未被使用的缓冲区
        if (b->refcnt == 0) {
            b->dev = dev;
            b->blockno = blockno;
            b->valid = 0;
            b->refcnt = 1;
            release(&bcache.lock);
            acquiresleep(&b->lock);
            return b;
        }
    }
    panic("bget: no buffers");
}

struct buf *bread(unsigned int dev, unsigned int blockno) {
    struct buf *b;

    // 获取缓冲区
    b = bget(dev, blockno);

    // 缓冲区无效，从磁盘读取
    if (!b->valid) {
        virtio_disk_rw(b, 0);
        // 缓冲区更改为有效
        b->valid = 1;
    }
    return b;
}

void bwrite(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("bwrite");
    
    // 将缓冲区数据写入磁盘
    virtio_disk_rw(b, 1);
}

void brelse(struct buf *b) {
    if (!holdingsleep(&b->lock))
        panic("brelse");

    // 释放缓冲区的睡眠锁
    releasesleep(&b->lock);

    acquire(&bcache.lock);
    // 减少引用计数
    b->refcnt--;
    // 引用计数为0说明缓冲区未被使用，移动到链表头部
    if (b->refcnt == 0) {
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.head.next;
        b->prev = &bcache.head;
        bcache.head.next->prev = b;
        bcache.head.next = b;
    }

    release(&bcache.lock);
}

void bpin(struct buf *b) {
    acquire(&bcache.lock);
    // 增加引用计数表示固定
    b->refcnt++;
    release(&bcache.lock);
}

void bunpin(struct buf *b) {
    acquire(&bcache.lock);
    b->refcnt--;
    release(&bcache.lock);
}

void bcache_reset(void) {
    acquire(&bcache.lock);
    // 重置缓冲区状态
    for (int i = 0; i < NBUF; i++) {
        bcache.buf[i].valid = 0;
        bcache.buf[i].refcnt = 0;
    }
    release(&bcache.lock);
}