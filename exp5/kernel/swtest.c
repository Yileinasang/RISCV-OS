#include "def.h"
#include "assert.h"
#include "riscv.h"

#define SBUF_SIZE 4
#define PRODUCE_COUNT 10

struct sbuf {
    int data[SBUF_SIZE];
    int head;
    int tail;
    int count;
    struct spinlock lock;
} sbuf;

void shared_buffer_init(void) {
    initlock(&sbuf.lock, "sbuf");
    sbuf.head = sbuf.tail = sbuf.count = 0;
}

void producer_task(void) {
    for (int i = 0; i < PRODUCE_COUNT; i++) {
        acquire(&sbuf.lock);
        while (sbuf.count == SBUF_SIZE) { sleep((void *)&sbuf, &sbuf.lock); }
        sbuf.data[sbuf.tail] = i;
        sbuf.tail = (sbuf.tail + 1) % SBUF_SIZE;
        sbuf.count++;
        printf("producer: put %d (count=%d)\n", i + 1, sbuf.count);
        wakeup((void *)&sbuf);
        release(&sbuf.lock);
    }
    printf("producer: completed\n");
    exit_process(current_proc, 0);
}

void consumer_task(void) {
    for (int k = 0; k < PRODUCE_COUNT; k++) {
        acquire(&sbuf.lock);
        while (sbuf.count == 0) { sleep((void *)&sbuf, &sbuf.lock); }
        int v = sbuf.data[sbuf.head];
        sbuf.head = (sbuf.head + 1) % SBUF_SIZE;
        sbuf.count--;
        printf("consumer: got %d (count=%d)\n", v + 1, sbuf.count);
        wakeup((void *)&sbuf);
        release(&sbuf.lock);
    }
    printf("consumer: completed\n");
    exit_process(current_proc, 0);
}
