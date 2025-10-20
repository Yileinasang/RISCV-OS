#include "spinlock.h"

void initlock(struct spinlock *lk) {
    lk->locked = 0;
}

void acquire(struct spinlock *lk) {
    while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
        ;
}

void release(struct spinlock *lk) {
    __sync_lock_release(&lk->locked);
}
