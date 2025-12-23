#include "riscv.h"
#include "def.h"

extern char etext[]; 

pagetable_t create_pagetable(void) {
    pagetable_t pt = (pagetable_t)alloc_page();
    if (!pt) return NULL; memset(pt, 0, PGSIZE); return pt;
}

int map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long size, int perm) {
    if((va % PGSIZE) != 0) panic("mappages: va not aligned");
    if((pa % PGSIZE) != 0) panic("mappages: pa not aligned");
    if((size % PGSIZE) != 0) panic("mappages: size not aligned");
    pte_t *pte; unsigned long a = va; unsigned long last = va + size - PGSIZE;
    for(;;){
        if((pte = walk_create(pt, a)) == 0) return -1; // 创建中间级页表失败    
        if(*pte & PTE_V) panic("mappages: remap");
        *pte = PA2PTE(pa) | perm | PTE_V;
        if(a == last) break; a += PGSIZE; pa += PGSIZE;
    }
    return 0;
}

void destroy_pagetable(pagetable_t pt) {
    for (int i = 0; i < 512; i++) {
        pte_t pte = pt[i];
        if ((pte & PTE_V) && !(pte & (PTE_R | PTE_W | PTE_X))) {
            pagetable_t child = (pagetable_t)PTE2PA(pte);
            destroy_pagetable(child);
        }
    }
    free_page(pt);
}

pte_t* walk_create(pagetable_t pt, unsigned long va) {
    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pt[PX(level, va)];
        if (*pte & PTE_V) { pt = (pagetable_t)PTE2PA(*pte); }
        else { pt = (pagetable_t)alloc_page(); if (!pt) return NULL; memset(pt, 0, PGSIZE); *pte = PA2PTE((unsigned long)pt) | PTE_V; }
    }
    return &pt[PX(0, va)];
}

pte_t* walk_lookup(pagetable_t pt, unsigned long va) {
    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pt[PX(level, va)]; if (*pte & PTE_V) pt = (pagetable_t)PTE2PA(*pte); else return NULL;
    }
    return &pt[PX(0, va)];
}

pagetable_t kernel_pagetable;

void kvm_init(void) {
    kernel_pagetable = create_pagetable();
    map_region(kernel_pagetable, KERNBASE, KERNBASE, (unsigned long)etext - KERNBASE, PTE_R | PTE_X);
    map_region(kernel_pagetable, (unsigned long)etext, (unsigned long)etext, PHYSTOP - (unsigned long)etext, PTE_R | PTE_W); 
    map_region(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W); 
}

void kvm_inithart(void) { w_satp(MAKE_SATP(kernel_pagetable)); sfence_vma(); }

void map_region(pagetable_t kpgtbl, unsigned long va, unsigned long pa, unsigned long sz, int perm) {
    if(map_page(kpgtbl, va, pa, sz, perm) != 0) panic("kvmmap");
}
