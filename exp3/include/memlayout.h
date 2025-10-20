#pragma once
#include "types.h"

// QEMU virt machine memory map
#define KERNBASE    0x80000000L   // DRAM base (kernel加载地址)


// UART0 MMIO base
#define UART0       0x10000000L

#define PGSIZE 4096UL
#define PGSHIFT 12

#define PA_BASE 0x80000000UL
#define PHYS_SIZE (128UL * 1024UL * 1024UL) // 128MB
#define PHYSTOP (PA_BASE + PHYS_SIZE)       // 0x88000000

extern char end[]; // 链接脚本提供

static inline uintptr_t pg_round_up(uintptr_t x) {
    return (x + PGSIZE - 1) & ~(PGSIZE - 1);
}
static inline uintptr_t pg_round_down(uintptr_t x) {
    return x & ~(PGSIZE - 1);
}
