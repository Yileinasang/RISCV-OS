#include "riscv.h"
#include "def.h"

extern char etext[]; 
extern char trampoline[];

pagetable_t create_pagetable(void) {
    pagetable_t pt = (pagetable_t)alloc_page();
    if (!pt) return NULL;
    memset(pt, 0, PGSIZE);
    return pt;
}

int map_page(pagetable_t pt, unsigned long va, unsigned long pa, unsigned long size, int perm) {
    if ((va % PGSIZE) != 0)
        panic("mappages: va not aligned");

    if ((pa % PGSIZE) != 0)
        panic("mappages: pa not aligned");

    if ((size % PGSIZE) != 0)
        panic("mappages: size not aligned");
    
    pte_t *pte;
    unsigned long a = va;
    unsigned long last = va + size - PGSIZE;
    for (;;){
        if ((pte = walk_create(pt, a)) == 0)
            // 创建中间级页表失败    
            return -1;
        if (*pte & PTE_V)
            // 页表项已存在
            panic("mappages: remap");

        // 更新页表项
        *pte = PA2PTE(pa) | perm | PTE_V;
        
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

void destroy_pagetable(pagetable_t pt) {
    for (int i = 0; i < 512; i++) {
        pte_t pte = pt[i];
        if ((pte & PTE_V) && !(pte & (PTE_R | PTE_W | PTE_X))) {
            // 中间级页表
            pagetable_t child = (pagetable_t)PTE2PA(pte);
            destroy_pagetable(child);
        }
    }
    free_page(pt);
}

// 必要时创建中间级页表
pte_t* walk_create(pagetable_t pt, unsigned long va) {
    for (int level = 2; level > 0; level--) {
        // 提取当前层级的页表项
        pte_t *pte = &pt[PX(level, va)];
        if (*pte & PTE_V) {
            // 页表项存在且有效，跳转到下一层页表
            pt = (pagetable_t)PTE2PA(*pte);
        } 
        else {
            // 页表项无效，分配新的页表页
            pt = (pagetable_t)alloc_page();
            // 分配失败
            if (!pt) 
                return NULL;  
            memset(pt, 0, PGSIZE);
            *pte = PA2PTE((unsigned long)pt) | PTE_V;
        }
    }
    return &pt[PX(0, va)];
}

// 不创建中间级页表
pte_t* walk_lookup(pagetable_t pt, unsigned long va) {
    for (int level = 2; level > 0; level--) {
        pte_t *pte = &pt[PX(level, va)];
        if (*pte & PTE_V) 
            // 页表项存在且有效，跳转到下一层页表
            pt = (pagetable_t)PTE2PA(*pte);
        else 
            return NULL;
    }
    return &pt[PX(0, va)];
}

pagetable_t kernel_pagetable;

void kvm_init(void) {
    kernel_pagetable = create_pagetable();

    // 映射内核代码段
    map_region(kernel_pagetable, KERNBASE, KERNBASE, 
                (unsigned long)etext - KERNBASE, PTE_R | PTE_X);

    // 映射内核数据段
    map_region(kernel_pagetable, (unsigned long)etext, (unsigned long)etext, 
               PHYSTOP - (unsigned long)etext, PTE_R | PTE_W); 

    // 映射设备
    map_region(kernel_pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W); 

    // 映射VirtIO MMIO
    map_region(kernel_pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    // 映射PLIC
    map_region(kernel_pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

    // 映射trampoline代码段
    map_region(kernel_pagetable, TRAMPOLINE, (unsigned long)trampoline, PGSIZE, PTE_R | PTE_X);

    // 映射内核栈
    proc_mapstacks(kernel_pagetable);
} 
 
void kvm_inithart(void) { 
    // 激活内核页表
    w_satp(MAKE_SATP(kernel_pagetable)); 
    sfence_vma(); 
}

void map_region(pagetable_t kpgtbl, unsigned long va, unsigned long pa, unsigned long sz, int perm) {
    // 建立映射
    if (map_page(kpgtbl, va, pa, sz, perm) != 0)
        panic("kvmmap");
}

void unmap_page(pagetable_t pagetable, unsigned long va, unsigned long npages, int do_free) {
    unsigned long a;
    pte_t *pte;

    if ((va % PGSIZE) != 0)
        panic("uvmunmap: not aligned");

    for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
        if ((pte = walk_create(pagetable, a)) == 0)
            continue;
        if ((*pte & PTE_V) == 0)
            continue;
        if (do_free) {
            unsigned long pa = PTE2PA(*pte);
            free_page((void *)pa);
        }
        *pte = 0;
    }
}

int copyout(pagetable_t pagetable, unsigned long dstva, const char *src, unsigned long len) {
    unsigned long va0, pa0, n;
    pte_t *pte;

    while (len > 0) {
        va0 = PGROUNDDOWN(dstva);

        // 查找页表项
        pte = walk_lookup(pagetable, va0);
        if (pte == 0)
            return -1;

        // 必须是有效用户页
        if (((*pte & PTE_V) == 0) || ((*pte & PTE_U) == 0))
            return -1;

        pa0 = PTE2PA(*pte);

        // 本页可拷贝的字节数
        n = PGSIZE - (dstva - va0);
        if (n > len)
            n = len;

        // 物理地址拷贝
        memmove((void *)(pa0 + (dstva - va0)), (const void *)src, n);

        len -= n;
        src += n;
        dstva = va0 + PGSIZE;
    }
    return 0;
}

int copy_pagetable(pagetable_t old, pagetable_t new, unsigned long sz) {
    pte_t *pte;
    unsigned long pa, i;
    unsigned int flags;
    char *mem;

    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = walk_create(old, i)) == 0)
            continue;
        if ((*pte & PTE_V) == 0)
            continue;
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = alloc_page()) == 0) {
            unmap_page(new, 0, i / PGSIZE, 1);
            return -1;
        }
        memmove(mem, (char *)pa, PGSIZE);
        if (map_page(new, i, PGSIZE, (unsigned long)mem, flags) != 0) {
            free_page(mem);
            unmap_page(new, 0, i / PGSIZE, 1);
            return -1;
        }
    }
    return 0; 
}