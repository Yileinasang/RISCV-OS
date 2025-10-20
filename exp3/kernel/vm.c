#include "types.h"
#include "vm.h"
#include "string.h"
#include "panic.h"

pagetable_t kernel_pagetable = 0;

void printf(const char *fmt, ...);
void panic(char *s);

// Adapt PMM calls to your interface
static void* alloc_pt_page(void) {
    void* p = alloc_page();
    if (p) memset(p, 0, PGSIZE);
    return p;
}
static void free_pt_page(void* p) {
    if (p) free_page(p);
}

static inline uint64 vpn_index(uint64 va, int level) {
    return VPN_MASK(va, level);
}

pte_t* walk_create(pagetable_t pt, uint64 va) {
    if (!pt) return 0;
    pagetable_t cur = pt;
    for (int level = 2; level > 0; level--) {
        uint64 idx = vpn_index(va, level);
        pte_t* pte = &cur[idx];
        if ((*pte & PTE_V) == 0) {
            void* child = alloc_pt_page();
            if (!child) return 0;
            uint64 pa = KERN_VA2PA((uint64)child);
            *pte = pa2pte(pa, PTE_V); // middle level: only V
        }
        if (!pte_is_table(*pte)) return 0; // conflict: leaf encountered
        uint64 child_pa = PTE_PA(*pte);
        cur = (pagetable_t)KERN_PA2VA(child_pa);
    }
    return &cur[vpn_index(va, 0)];
}

pte_t* walk_lookup(pagetable_t pt, uint64 va) {
    if (!pt) return 0;
    pagetable_t cur = pt;
    for (int level = 2; level > 0; level--) {
        pte_t* pte = &cur[vpn_index(va, level)];
        if ((*pte & PTE_V) == 0) return 0;
        if (!pte_is_table(*pte)) return 0;
        uint64 child_pa = PTE_PA(*pte);
        cur = (pagetable_t)KERN_PA2VA(child_pa);
    }
    return &cur[vpn_index(va, 0)];
}

pagetable_t create_pagetable(void) {
    void* root = alloc_pt_page();
    return (pagetable_t)root;
}

int map_page(pagetable_t pt, uint64 va, uint64 pa, int perm) {
    if ((va & (PGSIZE - 1)) || (pa & (PGSIZE - 1))) return -1;
    int leaf_perm = (perm | PTE_V);
    if ((leaf_perm & (PTE_R | PTE_W | PTE_X)) == 0) return -2;
    pte_t* pte = walk_create(pt, va);
    if (!pte) return -3;
    if (pte_is_leaf(*pte)) return -4; // already mapped
    *pte = pa2pte(pa, leaf_perm);
    return 0;
}

// Map a region [va, va+len) to [pa, pa+len), page-by-page
int map_region(pagetable_t pt, uint64 va, uint64 pa, uint64 len, int perm) {
    uint64 start_va = PGROUNDDOWN(va);
    uint64 start_pa = PGROUNDDOWN(pa);
    uint64 end_va   = PGROUNDUP(va + len);
    uint64 cur_va = start_va;
    uint64 cur_pa = start_pa;

    for (; cur_va < end_va; cur_va += PGSIZE, cur_pa += PGSIZE) {
        int rc = map_page(pt, cur_va, cur_pa, perm);
        if (rc != 0) return rc;
    }
    return 0;
}

static void destroy_recursive(pagetable_t pt, int level) {
    if (!pt) return;
    if (level == 0) {
        // 叶子层：不递归
        return;
    }

    // 遍历当前页表的 512 项
    for (int i = 0; i < 512; i++) {
        pte_t p = pt[i];
        if ((p & PTE_V) == 0) continue;
        if (pte_is_table(p)) {
            uint64 child_pa = PTE_PA(p);
            pagetable_t child = (pagetable_t)KERN_PA2VA(child_pa);
            destroy_recursive(child, level - 1);
            // 释放中间级页表页
            free_pt_page((void*)child);
            pt[i] = 0;
        } else {
            // 叶子映射：保留或清零（不释放物理页）
            pt[i] = 0;
        }
    }
}

void destroy_pagetable(pagetable_t pt) {
    if (!pt) return;
    destroy_recursive(pt, 2);
    free_pt_page((void*)pt);
}

// Debug dump
static void dump_indent(int depth) {
    for (int i = 0; i < depth; i++) printf("  ");
}
static void dump_pte_line(int depth, int idx, pte_t p) {
    dump_indent(depth);
    if (!(p & PTE_V)) {
        printf("[%d] PTE=0x%lx (invalid)\n", idx, p);
        return;
    }
    uint64 pa = PTE_PA(p);
    if (pte_is_table(p)) {
        printf("[%d] PTE=0x%lx TABLE pa=0x%lx\n", idx, p, pa);
    } else if (pte_is_leaf(p)) {
        printf("[%d] PTE=0x%lx LEAF pa=0x%lx perms=", idx, p, pa);
        if (p & PTE_R) printf("R");
        if (p & PTE_W) printf("W");
        if (p & PTE_X) printf("X");
        if (p & PTE_U) printf("U");
        printf("\n");
    } else {
        printf("[%d] PTE=0x%lx (reserved/unknown)\n", idx, p);
    }
}
void dump_pagetable(pagetable_t pt, int level) {
    if (!pt) { printf("pagetable: (null)\n"); return; }
    printf("=== dump pagetable level=%d ===\n", level);
    for (int i = 0; i < 512; i++) {
        pte_t p = pt[i];
        if ((p & PTE_V) == 0) continue;
        dump_pte_line(0, i, p);
        if (pte_is_table(p) && level > 0) {
            uint64 child_pa = PTE_PA(p);
            pagetable_t child = (pagetable_t)KERN_PA2VA(child_pa);
            for (int j = 0; j < 512; j++) {
                pte_t cp = child[j];
                if ((cp & PTE_V) == 0) continue;
                dump_pte_line(1, j, cp);
                if (pte_is_table(cp) && level - 1 > 0) {
                    uint64 gpa = PTE_PA(cp);
                    pagetable_t grand = (pagetable_t)KERN_PA2VA(gpa);
                    for (int k = 0; k < 512; k++) {
                        pte_t gp = grand[k];
                        if ((gp & PTE_V) == 0) continue;
                        dump_pte_line(2, k, gp);
                    }
                }
            }
        }
    }
    printf("=== end dump ===\n");
}

// CSR helpers: write satp, and fence TLB
void w_satp(uint64 x) {
    asm volatile("csrw satp, %0" :: "r"(x));
    asm volatile("csrr zero, satp"); // serialize (optional)
}
void sfence_vma(void) {
    asm volatile("sfence.vma zero, zero");
}

// Linker provides etext; memlayout.h provides KERNBASE/PHYSTOP/UART0
extern char etext[];

// Kernel VM init: build kernel pagetable with identity mappings
void kvminit(void) {
    kernel_pagetable = create_pagetable();
    if (!kernel_pagetable) panic("kvminit: root pagetable alloc failed");

    // Align boundaries
    uint64 k_text_start = (uint64)KERNBASE;
    uint64 k_text_end   = PGROUNDUP((uint64)etext);
    uint64 k_data_start = k_text_end;
    uint64 k_data_end   = (uint64)PHYSTOP;

    // 1) Map .text as RX
    if (map_region(kernel_pagetable,
                   k_text_start, k_text_start,
                   k_text_end - k_text_start, PTE_R | PTE_X) != 0)
        panic("kvminit: map .text failed");

    // 2) Map .data/.bss/rest as RW
    if (map_region(kernel_pagetable,
                   k_data_start, k_data_start,
                   k_data_end - k_data_start, PTE_R | PTE_W) != 0)
        panic("kvminit: map .data failed");

    // 3) Map UART (MMIO) as RW, non-executable
    if (map_region(kernel_pagetable, (uint64)UART0, (uint64)UART0,
                   PGSIZE, PTE_R | PTE_W) != 0)
        panic("kvminit: map UART failed");
}


// Activate kernel pagetable on this hart
void kvminithart(void) {
    uint64 satp = MAKE_SATP(kernel_pagetable);
    w_satp(satp);
    sfence_vma();
}
