# 实验三:页表与内存管理

## 一、实验目的
本实验旨在实现一个简化操作系统内核的内存子系统，具体目标包括：
1. 构建物理内存分配器（PMM），支持页级分配与释放
2. 实现页表管理器（VM），支持多级页表的创建、映射、查找与销毁
3. 启用虚拟内存，通过构建内核页表并写入 satp 寄存器，完成内核在分页模式下的运行
4. 对比 xv6 的实现，理解其设计思路，并在此基础上进行优化和差异化设计

## 二、实验环境
- 硬件平台：QEMU virt (RISC-V 64)
- 编译工具链：riscv64-unknown-elf-gcc
- 内存布局：
  - DRAM 起始地址：0x80000000
  - UART0 MMIO 地址：0x10000000
  - 内核大小：128MB
- 链接脚本：kernel.ld，导出 etext、end 等符号
- 代码结构：
  - pmm.c / pmm.h：物理内存管理
  - vm.c / vm.h：页表管理与虚拟内存初始化
  - memlayout.h：内存布局常量
  - kernel.ld：链接脚本

## 三、实验过程

### 1. 物理内存分配器
- 实现思路：采用空闲链表管理物理页，提供 alloc_page()、free_page()、alloc_pages(n) 等接口
- 测试方法：在 main.c 中分配单页与多页，检查空闲页数是否正确变化；释放后空闲页数是否恢复
- 结果：分配与释放均正确，PMM 功能正常

### 2. 页表管理器
- 核心接口：
  - create_pagetable()：创建根页表
  - map_page()：建立单页映射，检测重复映射
  - map_region()：支持区间映射，自动对齐页边界
  - walk_create()：逐级下降，必要时分配中间页表
  - walk_lookup()：查找虚拟地址对应的 PTE
  - destroy_pagetable()：递归释放页表结构
  - dump_pagetable()：递归打印页表结构，便于调试
- 实现要点：
  - 遵循 RISC-V Sv39 三层页表结构，每级 9 位索引
  - 中间级 PTE 仅置 V 位，不带 R/W/X
  - 叶子 PTE 设置物理页基址和权限位
- 测试结果：
  - 成功映射虚拟地址到物理地址
  - 重复映射返回错误码
  - walk_lookup 能正确找到已映射页，未映射页返回空
  - destroy_pagetable 能释放页表结构

### 3. 启用虚拟内存
- 内核页表构建 (kvminit)：
  - 映射 .text 段为只读+可执行（R|X）
  - 映射 .data/.bss 段为读写（R|W）
  - 映射设备 MMIO（UART0）为读写（R|W），禁止执行
  - 使用 PGROUNDUP(etext) 对齐，避免 .text 与 .data 重叠
- 页表激活 (kvminithart)：
  - 构造 satp 值：MODE=8 (Sv39)，ASID=0，PPN=根页表物理页号
  - 写入 satp，执行 sfence.vma 刷新 TLB
- 测试结果：
  - 内核在分页模式下继续运行
  - 页表打印显示 .text、.data、设备区域均已正确映射

## 四、与 xv6 的对比

### 相同点
- 使用 Sv39 三层页表
- 内核页表采用恒等映射，简化分页启用过程
- 分为 kvminit 和 kvminithart 两步

### 不同点
- 重复映射检测：我的实现返回明确错误码，xv6 直接覆盖
- 权限分离：.text 只读+可执行，.data 读写不可执行
- 调试接口：提供 dump_pagetable，便于观察页表结构
- 模块化：PMM 与 VM 分离，接口清晰

## 五、实验总结
- 完成内容：
  - 实现了物理内存分配器
  - 实现了页表管理器
  - 构建并启用了内核页表，进入分页模式
- 遇到问题：
  - .text 与 .data 边界未对齐导致重复映射 → 使用 PGROUNDUP(etext) 修复
  - PHYSTOP 定义冲突 → 统一为单一定义
  - 链接脚本未导出 etext → 在 .text 段末尾添加 PROVIDE(etext = .)
- 最终结果：内核在分页模式下运行正常，页表映射正确

## 六、展望
- 更细粒度权限：为 .rodata 单独映射为只读
- 设备映射扩展：加入 PLIC、CLINT、trampoline
- 性能优化：支持大页映射（2MB/1GB）
- 用户态支持：为进程构建独立页表，利用 ASID 减少 TLB flush