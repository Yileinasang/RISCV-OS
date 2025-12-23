#include "riscv.h"
#include "def.h"

// 页面池容量
#define PAGE_POOL_CAP 16

// kernel.ld定义的内核代码结束地址
extern char end[];

struct run {
	struct run *next;
};

struct {
	struct spinlock lock;
	struct run *freelist;
} pmm;

struct {
    struct spinlock lock;
    void *buf[PAGE_POOL_CAP];
    int n;
} page_cache;

void pmm_init(void) {
	initlock(&pmm.lock, "pmm");
    initlock(&page_cache.lock, "page_cache");
    page_cache.n = 0;
    freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
    // 对齐起始地址
	char *start = (char *)PGROUNDUP((unsigned long)pa_start);
    for (char *p = (char *)pa_end - PGSIZE; p >= start; p -= PGSIZE) {
        free_page(p);
    }
}

void *alloc_page(void) {
	// 优先从页面池分配
    acquire(&page_cache.lock);
    if (page_cache.n > 0) {
        void *p = page_cache.buf[--page_cache.n];
        release(&page_cache.lock);
        // 填充垃圾数据
        memset((char*)p, 5, PGSIZE);
        return p;
    }
    release(&page_cache.lock);
    
    // 页面池为空，从空闲页链表分配
    struct run *r;
	acquire(&pmm.lock);
	r = pmm.freelist;
	if (r) 
		pmm.freelist = r->next;
	release(&pmm.lock);
	if (r)
        // 填充垃圾数据
		memset((char *)r, 5, PGSIZE);
	return (void *)r;
}

void free_page(void *page) {
    // 检查地址合法性
	if (((unsigned long)page % PGSIZE) != 0 || (char *)page < end || (unsigned long)page >= PHYSTOP)
		panic("free_page: invalid page");

    // 填充垃圾数据
	memset(page, 1, PGSIZE);
    // 优先回收到页面池
    acquire(&page_cache.lock);
    if (page_cache.n < PAGE_POOL_CAP) {
        page_cache.buf[page_cache.n++] = page;
        release(&page_cache.lock);
        return;
    }
    release(&page_cache.lock);

    // 页面池已满，回收到空闲页链表
    free_page_to_freelist(page);
}

void free_page_to_freelist(void *page) {
    struct run *r = (struct run*)page;

    acquire(&pmm.lock);

    // 按物理地址有序插入，保证freelist从低地址到高地址，能够分配连续页面
    if (pmm.freelist == NULL || (char*)r < (char*)pmm.freelist) {
        r->next = pmm.freelist;
        pmm.freelist = r;
    } 
    else {
        struct run *prev = pmm.freelist;
        while (prev->next && (char*)prev->next < (char*)r) {
            prev = prev->next;
        }
        r->next = prev->next;
        prev->next = r;
    }
    release(&pmm.lock);
}

void *alloc_pages(int n) {
    if (n <= 0) 
        return NULL;

    acquire(&pmm.lock);

    struct run *prev = NULL;
    struct run *cur  = pmm.freelist;

    // 候选连续区间的起点、起点前驱、尾节点、长度
    struct run *seg_start = NULL;
    struct run *seg_prev  = NULL;
    struct run *seg_end   = NULL;
    int seg_len = 0;

    while (cur) {
        if (seg_len == 0) {
            // 新的候选区间
            seg_start = cur;
            seg_prev  = prev;
            seg_end   = cur;
            seg_len   = 1;
        } 
        else if ((char*)cur == (char*)seg_end + PGSIZE) {
            // 当前节点与区间末尾连续，延长区间
            seg_end = cur;
            seg_len++;
        } 
        else {
            // 不连续，从当前节点开始新的候选区间
            seg_start = cur;
            seg_prev  = prev;
            seg_end   = cur;
            seg_len   = 1;
        }

        if (seg_len == n) {
            // 找到n个连续页面
            struct run *cut_end = seg_start;
            for (int i = 1; i < n; i++) {
                cut_end = cut_end->next;
            }
            struct run *rest = cut_end->next;   // 剩余链表起点
            // 从空闲页链表中删除连续页面
            if (seg_prev) 
                seg_prev->next = rest;
            else          
                pmm.freelist = rest;

            // 断开连续页面
            cut_end->next = NULL; 

            release(&pmm.lock);

            // 填充垃圾数据
            for (int i = 0; i < n; i++) {
                memset((void*)((char*)seg_start + i*PGSIZE), 3, PGSIZE);
            }

            return (void*)seg_start; // 返回连续页面的起始地址
        }

        // 遍历空闲页链表
        prev = cur;
        cur  = cur->next;
    }

    release(&pmm.lock);
    panic("alloc_pages: not enough free pages\n");
    return NULL; 
}