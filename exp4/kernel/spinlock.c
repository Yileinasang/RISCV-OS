#include "def.h"

void initlock(struct spinlock *lk, char *name) {
    lk->name = name;
    lk->locked = 0;
}

void acquire(struct spinlock *lk) {
    // 自旋等待
    while (__sync_lock_test_and_set(&lk->locked, 1) != 0);

    // 确保内存操作不会被重新排序
    __sync_synchronize();
}

void release(struct spinlock *lk) {
    // 确保所有内存操作在释放锁之前完成
    __sync_synchronize();

    // 释放锁
    __sync_lock_release(&lk->locked);
}