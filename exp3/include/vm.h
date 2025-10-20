#pragma once

#include "types.h"
#include "memlayout.h"
#include "pmm.h"

// Sv39 constants
#ifndef PGSIZE
#define PGSIZE 4096
#endif

typedef uint64 pte_t;
typedef uint64* pagetable_t;

// PTE flags
#define PTE_V (1L << 0)
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)
#define PTE_G (1L << 5)
#define PTE_A (1L << 6)
#define PTE_D (1L << 7)

#define PTE_PPN_SHIFT 10
#define PTE_ADDR_SHIFT 12
#define PTE_INDEX_BITS 9
#define PTE_INDEX_MASK 0x1FF

// Index macros
#define VPN_SHIFT(level) (12 + 9 * (level))
#define VPN_MASK(va, level) (((va) >> VPN_SHIFT(level)) & 0x1FF)

// Align & address macros
#define PGROUNDUP(sz)   (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a)  ((a) & ~(PGSIZE-1))
#define PTE_PA(pte)     (((pte) >> PTE_PPN_SHIFT) << PTE_ADDR_SHIFT)

static inline pte_t pa2pte(uint64 pa, int perm) {
    return ((pa >> PTE_ADDR_SHIFT) << PTE_PPN_SHIFT) | (perm & 0x3FF);
}

static inline int pte_is_table(pte_t p) {
    return (p & PTE_V) && ((p & (PTE_R|PTE_W|PTE_X)) == 0);
}

static inline int pte_is_leaf(pte_t p) {
    return (p & PTE_V) && (p & (PTE_R|PTE_W|PTE_X));
}

// Kernel PA<->VA (adjust if you use an offset)
#ifndef KERN_PA2VA
#define KERN_PA2VA(pa) ((uint64)(pa))
#endif
#ifndef KERN_VA2PA
#define KERN_VA2PA(va) ((uint64)(va))
#endif

// Core interfaces
pagetable_t create_pagetable(void);
int map_page(pagetable_t pt, uint64 va, uint64 pa, int perm);
void destroy_pagetable(pagetable_t pt);

// Helpers
pte_t* walk_create(pagetable_t pt, uint64 va);
pte_t* walk_lookup(pagetable_t pt, uint64 va);

// Range mapping
int map_region(pagetable_t pt, uint64 va, uint64 pa, uint64 len, int perm);

// Debug
void dump_pagetable(pagetable_t pt, int level);

// Kernel VM init/activate
void kvminit(void);
void kvminithart(void);

// SATP (Sv39) fields: MODE[63:60] | ASID[59:44] | PPN[43:0]
#define SATP_MODE_SV39  (8ULL)
#define SATP_ASID_SHIFT (44ULL)
#define SATP_MODE_SHIFT (60ULL)
static inline uint64 MAKE_SATP(pagetable_t root) {
    uint64 root_pa = KERN_VA2PA((uint64)root);
    uint64 ppn = root_pa >> PTE_ADDR_SHIFT; // 4K pages -> shift 12
    return (SATP_MODE_SV39 << SATP_MODE_SHIFT) | (0ULL << SATP_ASID_SHIFT) | ppn;
}

// CSR helpers
void w_satp(uint64 x);
void sfence_vma(void);

// kernel pagetable global
extern pagetable_t kernel_pagetable;

// linker symbol for end of .text
extern char etext[];
