#include "riscv.h"
#include "def.h"

#define PAGE_POOL_CAP 16
extern char end[];

struct run { struct run *next; };

struct { struct spinlock lock; struct run *freelist; } pmm;
struct { struct spinlock lock; void *buf[PAGE_POOL_CAP]; int n; } page_cache;

void pmm_init(void) {
    initlock(&pmm.lock, "pmm");
    initlock(&page_cache.lock, "page_cache");
    page_cache.n = 0;
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
    char *start = (char *)PGROUNDUP((unsigned long)pa_start);
    for (char *p = (char *)pa_end - PGSIZE; p >= start; p -= PGSIZE) { free_page(p); }
}

void *alloc_page(void) {
    acquire(&page_cache.lock);
    if (page_cache.n > 0) { void *p = page_cache.buf[--page_cache.n]; release(&page_cache.lock); memset((char*)p, 5, PGSIZE); return p; }
    release(&page_cache.lock);
    struct run *r; acquire(&pmm.lock); r = pmm.freelist; if (r) pmm.freelist = r->next; release(&pmm.lock); if (r) memset((char *)r, 5, PGSIZE); return (void *)r;
}

void free_page(void *page) {
    if (((unsigned long)page % PGSIZE) != 0 || (char *)page < end || (unsigned long)page >= PHYSTOP) panic("free_page: invalid page");
    memset(page, 1, PGSIZE);
    acquire(&page_cache.lock);
    if (page_cache.n < PAGE_POOL_CAP) { page_cache.buf[page_cache.n++] = page; release(&page_cache.lock); return; }
    release(&page_cache.lock);
    free_page_to_freelist(page);
}

void free_page_to_freelist(void *page) {
    struct run *r = (struct run*)page; acquire(&pmm.lock);
    if (pmm.freelist == NULL || (char*)r < (char*)pmm.freelist) { r->next = pmm.freelist; pmm.freelist = r; }
    else { struct run *prev = pmm.freelist; while (prev->next && (char*)prev->next < (char*)r) { prev = prev->next; } r->next = prev->next; prev->next = r; }
    release(&pmm.lock);
}

void *alloc_pages(int n) {
    if (n <= 0) return NULL; acquire(&pmm.lock);
    struct run *prev = NULL; struct run *cur  = pmm.freelist;
    struct run *seg_start = NULL; struct run *seg_prev  = NULL; struct run *seg_end   = NULL; int seg_len = 0;
    while (cur) {
        if (seg_len == 0) { seg_start = cur; seg_prev  = prev; seg_end   = cur; seg_len   = 1; }
        else if ((char*)cur == (char*)seg_end + PGSIZE) { seg_end = cur; seg_len++; }
        else { seg_start = cur; seg_prev  = prev; seg_end   = cur; seg_len   = 1; }
        if (seg_len == n) {
            struct run *cut_end = seg_start; for (int i = 1; i < n; i++) { cut_end = cut_end->next; }
            struct run *rest = cut_end->next; if (seg_prev) seg_prev->next = rest; else pmm.freelist = rest; cut_end->next = NULL; release(&pmm.lock);
            for (int i = 0; i < n; i++) { memset((void*)((char*)seg_start + i*PGSIZE), 3, PGSIZE); }
            return (void*)seg_start;
        }
        prev = cur; cur  = cur->next;
    }
    release(&pmm.lock); panic("alloc_pages: not enough free pages\n"); return NULL; 
}
