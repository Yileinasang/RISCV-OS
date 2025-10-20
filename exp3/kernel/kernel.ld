OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
  /* 起始地址：QEMU virt 典型地址 */
  . = 0x80000000;

  /* 代码 */
  .text : {
    KEEP(*(.text.start))
    *(.text .text.*)
    PROVIDE(etext = .);
  }

  /* 只读数据 */
  .rodata : {
    *(.rodata .rodata.*)
  }

  /* 已初始化数据 */
  .data : {
    *(.data .data.*)
  }

  /* BSS：未初始化数据，清零区间符号 */
  .bss : {
    . = ALIGN(16);
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    . = ALIGN(16);
    __bss_end = .;
  }

  /* 栈：放在镜像末尾，16KB */
  . = ALIGN(16);
  __stack_bottom = .;
  . += 0x4000;
  __stack_top = .;

  /* 导出内核范围符号 */
  __kernel_start = 0x80000000;
  __kernel_end   = .;
  PROVIDE(end = .);
}
