# 实验报告（exp3：页表与内存管理）

## 实验概述
- 实验目标：完成物理内存分配器与 Sv39 映射，启用分页后保持 UART/内核可访问。
- 完成情况：`freelist` 分配/释放、三级页表创建/映射/销毁、`satp+sfence` 启用分页均通过自测。
- 开发环境：Ubuntu 22.04；riscv64-unknown-elf-gcc 12.x；qemu-system-riscv64 7.x。

## 关键函数实现
- 物理页分配（`kernel/pmm.c`）：空闲链表取头。
```c
void *alloc_page(void){ acquire(&lock); struct run *r=freelist; if(r)freelist=r->next; release(&lock); return r; }
```
- 页表遍历（`kernel/vm.c`）：创建中间页表。
```c
pte_t* walk_create(pagetable_t pt, uint64 va){
  for(int lvl=2; lvl>0; --lvl){ pte_t *pte=&pt[PX(lvl,va)];
    if(*pte&PTE_V) pt=(pagetable_t)PTE2PA(*pte);
    else{ pagetable_t n=alloc_page(); memset(n,0,PGSIZE); *pte=PA2PTE(n)|PTE_V; pt=n; }
  }
  return &pt[PX(0,va)];
}
```
- 内核映射（`kernel/vm.c`）：构建等映射并启用分页。
```c
void kvminit(void){ kernel_pagetable=create_pagetable(); map_region(kernel_pagetable,KERNBASE,KERNBASE,PHYSTOP-KERNBASE,PTE_R|PTE_W|PTE_X|PTE_V);} 
```

## 难点突破
- 对齐与重复映射：`map_page` 检查 VA/PA 对齐并返回错误码定位问题。
- satp 切换安全：启用前确保 UART/代码/数据已映射并执行 `sfence.vma`。

## 源码理解与对比
- 相同：free list + 三级页表接口与 xv6 相近。
- 不同：暂不支持大页与高半内核；未加入引用计数与填充图案。

## 测试情况
- 功能：`make && make run`，测试分配/释放、映射/解映射及页表打印。
- 边界：重复映射与非对齐应返回错误；未映射访问触发 page fault。

## 思考题与回答
- 设计对比：
  - 与 xv6 的不同：采用空闲链表/位图；本实验用链表实现便于调试。
  - 权衡：链表简单但易碎片化；位图扫描成本高。
- 内存安全：
  - 防恶意：大小/对齐校验、双重释放检测与断言；越界由页表保护。
  - 权限：U/S、R/W/X、AD 位合理设置，禁用用户态执行内核页。
- 性能分析：
  - 瓶颈：TLB 未命中与锁竞争；
  - 优化：`rdcycle` 计时、批量映射、减小锁粒度。
- 扩展性：
  - 用户进程：独立页表/陷入返回/系统调用入口；分离内核/用户栈。
  - 共享与写时复制：fork 共享只读页，写入触发缺页复制，维护引用计数。
- 错误恢复：
  - 页表创建失败回滚已分配资源；
  - 泄漏检测：分配计数对比与审计。
