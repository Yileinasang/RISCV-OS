#pragma once
#include "types.h"

struct spinlock {
    volatile int locked;
};

void initlock(struct spinlock *lk);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);
