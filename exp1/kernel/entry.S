    .section .text.start
    .globl _start
_start:

    # 调试：上电即向 UART 输出 'S'
    li   t0, 0x10000000       # UART 基地址
    li   t1, 'S'
    sb   t1, 0(t0)

    # 1) 设置栈指针
    .option nopic
    la   sp, __stack_top

    # 调试：栈设置完成输出 'P'
    li   t1, 'P'
    sb   t1, 0(t0)

    # 2) 清零 BSS：从 __bss_start 到 __bss_end
    la   t2, __bss_start
    la   t3, __bss_end
clear_bss:
    bgeu t2, t3, bss_done
    sd   zero, 0(t2)           # 64 位写零（rv64）
    addi t2, t2, 8
    j    clear_bss
bss_done:

    # 3) 跳转到 C 入口（不返回）
    la   t4, kernel_main
    jr   t4

    # 如果不小心返回，直接死循环
hang:
    wfi
    j    hang
