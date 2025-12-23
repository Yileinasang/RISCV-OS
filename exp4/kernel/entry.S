.section .text
.global _entry

_entry:
    la sp, stack_top
    li a0, 1024*4
    li a1, 1
    mul a0, a0, a1
    add sp, sp, a0
    call main

spin:
    j spin