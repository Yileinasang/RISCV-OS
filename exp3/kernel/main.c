#include "types.h"
#include "spinlock.h"
#include "memlayout.h"
#include "pmm.h"
#include "vm.h"

void printf(const char *fmt, ...);
void panic(char *s);

// 简单断言：失败则 panic
static void kassert(int cond, const char *msg) {
    if (!cond) panic((char*)msg);
}

// 调试打印映射结果
static void print_map_result(const char* name, int rc) {
    if (rc == 0) {
        printf("%s: success\n", name);
    } else {
        printf("%s: failed rc=%d\n", name, rc);
    }
}

// 物理内存分配器测试
static void test_physical_memory(void) {
    printf("[TEST] physical memory...\n");
    void *page1 = alloc_page();
    void *page2 = alloc_page();
    printf("  alloc page1=%p page2=%p\n", page1, page2);
    kassert((page1 != 0) && (page2 != 0), "alloc_page returned null");
    kassert(page1 != page2, "alloc_page returned duplicate page");
    kassert((((uint64)page1 & (PGSIZE-1)) == 0) && (((uint64)page2 & (PGSIZE-1)) == 0),
            "alloc_page not page-aligned");

    // 写入验证
    *(int*)page1 = 0x12345678;
    kassert(*(int*)page1 == 0x12345678, "page write/read failed");

    // 释放并再次分配
    free_page(page1);
    printf("  free page1=%p\n", page1);
    void *page3 = alloc_page();
    printf("  re-alloc page3=%p\n", page3);
    kassert(page3 != 0, "alloc_page after free failed");

    // 清理
    free_page(page2);
    free_page(page3);
    printf("  free page2=%p page3=%p\n", page2, page3);
    printf("[TEST] physical memory passed. free=%d\n", (int)pmm_free_pages());
}

// 页表功能测试
static void test_pagetable(void) {
    printf("[TEST] pagetable...\n");
    pagetable_t pt = create_pagetable();
    kassert(pt != 0, "create_pagetable failed");

    uint64 va = 0x1000000UL; // 16MB, 对齐页
    uint64 pa = (uint64)alloc_page();
    kassert(pa != 0, "alloc_page for map failed");

    int rc = map_page(pt, va, pa, PTE_R | PTE_W);
    print_map_result("map basic", rc);
    kassert(rc == 0, "map_page basic failed");

    // 检查 walk_lookup
    pte_t *pte = walk_lookup(pt, va);
    kassert(pte && (*pte & PTE_V), "walk_lookup failed");
    kassert(PTE_PA(*pte) == pa, "PTE_PA mismatch");
    kassert((*pte & PTE_R) && (*pte & PTE_W) && !(*pte & PTE_X), "PTE perm mismatch");

    // 重复映射应失败
    int rc_dup = map_page(pt, va, pa, PTE_R | PTE_W);
    kassert(rc_dup != 0, "duplicate map should fail");

    dump_pagetable(pt, 2);

    destroy_pagetable(pt);
    free_page((void*)pa);
    printf("[TEST] pagetable passed.\n");
}

// 虚拟内存启用测试
static void test_virtual_memory(void) {
    printf("[TEST] virtual memory...\n");
    printf("Before enabling paging...\n");
    kvminit();
    kvminithart();
    printf("After enabling paging...\n");
    // 简单访问验证：读取 UART 基址不会异常
    (void)*(volatile uint8*)UART0;
    printf("[TEST] virtual memory passed.\n");
}

// xv6-like kernel entry
void main(void) {
    printf("xv6-like kernel starting...\n");

    pmm_init();
    printf("PMM initialized: total=%d pages, free=%d pages\n",
           (int)pmm_total_pages(), (int)pmm_free_pages());

    test_physical_memory();
    test_pagetable();
    test_virtual_memory();

    printf("Kernel main finished.\n");
    for (;;) {
        // idle loop
    }
}
