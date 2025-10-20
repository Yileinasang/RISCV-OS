#include "types.h"
#include "memlayout.h"
#include "pmm.h"
#include "spinlock.h"

void printf(const char *fmt, ...);
void panic(char *s);
struct run {
    struct run *next;
};

static struct run *freelist;
static struct spinlock lock;

static size_t total_pages;
static size_t free_pages_cnt;

extern char end[]; // 内核结束

void pmm_init(void) {
    initlock(&lock);
    uintptr_t pa_start = pg_round_up((uintptr_t)end);
    uintptr_t pa_end   = pg_round_down(PHYSTOP);

    freelist = 0;
    total_pages = (pa_end - pa_start) / PGSIZE;
    free_pages_cnt = 0;

    for(uintptr_t p=pa_start; p+PGSIZE<=pa_end; p+=PGSIZE){
        free_page((void*)p);
    }
}

void* alloc_page(void) {
    acquire(&lock);
    struct run *r = freelist;
    if(r) {
        freelist = r->next;
        free_pages_cnt--;
    }
    release(&lock);
    return (void*)r;
}

void free_page(void* pa) {
    if(((uintptr_t)pa % PGSIZE) != 0) panic("free_page: not aligned");
    acquire(&lock);
    struct run *r = (struct run*)pa;
    r->next = freelist;
    freelist = r;
    free_pages_cnt++;
    release(&lock);
}

void* alloc_pages(int n) {
    if(n==1) return alloc_page();
    // 简化：逐页分配，不保证连续
    void* first = alloc_page();
    for(int i=1;i<n;i++) alloc_page();
    return first;
}

size_t pmm_total_pages(void) { return total_pages; }
size_t pmm_free_pages(void) { return free_pages_cnt; }

