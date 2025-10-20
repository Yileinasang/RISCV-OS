#include "types.h"
#include "spinlock.h"
#include "memlayout.h"
#include "pmm.h"
#include "vm.h"

void printf(const char *fmt, ...);
void panic(char *s);

static void print_map_result(const char* name, int rc) {
    if (rc == 0) {
        printf("%s: success\n", name);
    } else {
        printf("%s: failed rc=%d\n", name, rc);
    }
}

// xv6-like kernel entry
void main(void) {
    printf("xv6-like kernel starting...\n");

    // 1) Initialize PMM and do basic allocation tests
    pmm_init();
    printf("PMM initialized: total=%d pages, free=%d pages\n",
           (int)pmm_total_pages(), (int)pmm_free_pages());

    void *p1 = alloc_page();
    printf("Allocated one page at %p\n", p1);
    printf("Free pages now: %d\n", (int)pmm_free_pages());

    void *p4 = alloc_pages(4);
    printf("Allocated 4 pages starting at %p\n", p4);
    printf("Free pages now: %d\n", (int)pmm_free_pages());

    free_page(p1);
    printf("Freed one page, free pages: %d\n", (int)pmm_free_pages());

    // Note: free_page(p4) frees only the first of 4 pages in this simple test
    free_page(p4);
    printf("Freed first page of 4-page block, free pages: %d\n", (int)pmm_free_pages());

    // 2) Pagetable creation and mapping tests
    pagetable_t pt = create_pagetable();
    if (!pt) panic("create_pagetable failed");
    printf("Created new pagetable at %p\n", pt);

    // Allocate two physical pages we will map into our pagetable
    void *kva_a = alloc_page();
    void *kva_b = alloc_page();
    if (!kva_a || !kva_b) panic("alloc_page for mapping failed");
    printf("Mapping test pages: A=%p, B=%p\n", kva_a, kva_b);

    // Choose virtual addresses (page-aligned) to map
    // Pick a high kernel VA region for testing; adjust if your layout differs
    uint64 va_base = PGROUNDDOWN(0x40000000UL);
    uint64 va_a    = va_base;
    uint64 va_b    = va_base + PGSIZE;

    // Convert kernel VA of allocated pages to PA (based on your mapping macros)
    uint64 pa_a = KERN_VA2PA((uint64)kva_a);
    uint64 pa_b = KERN_VA2PA((uint64)kva_b);

    // Permissions: kernel leaf, read-write (no execute, no user)
    int perms = PTE_R | PTE_W;

    // 2.1 Map first page
    int rc = map_page(pt, va_a, pa_a, perms);
    print_map_result("map va_a->pa_a", rc);

    // 2.2 Duplicate map (should fail with conflict)
    int rc_dup = map_page(pt, va_a, pa_a, perms);
    print_map_result("duplicate map va_a", rc_dup);

    // 2.3 Map second page next to the first
    int rc2 = map_page(pt, va_b, pa_b, perms);
    print_map_result("map va_b->pa_b", rc2);

    // 2.4 Dump pagetable structure
    printf("Dumping pagetable after mappings:\n");
    dump_pagetable(pt, 2);

    // 2.5 Optional: probe walk_lookup on mapped and unmapped addresses
    pte_t* pte_a = walk_lookup(pt, va_a);
    printf("walk_lookup(va_a): %s, pte=%p\n", pte_a ? "found" : "not found", pte_a);

    pte_t* pte_b = walk_lookup(pt, va_b);
    printf("walk_lookup(va_b): %s, pte=%p\n", pte_b ? "found" : "not found", pte_b);

    pte_t* pte_c = walk_lookup(pt, va_b + PGSIZE);
    printf("walk_lookup(va_b+PGSIZE): %s\n", pte_c ? "found" : "not found");

    // 3) Destroy pagetable (frees page table pages; does not free mapped data pages)
    destroy_pagetable(pt);
    printf("Pagetable destroyed.\n");

    // Free the data pages we used for mapping
    free_page(kva_a);
    free_page(kva_b);
    printf("Freed mapped data pages. Free pages: %d\n", (int)pmm_free_pages());

    printf("Kernel main finished.\n");

    pmm_init();
    kvminit();
    kvminithart();


    for (;;) {
        // idle loop
    }
}
