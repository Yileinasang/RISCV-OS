#pragma once
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int  uint32;
typedef unsigned long uint64;

// 固定宽度与变参支持（避免依赖标准头）
typedef unsigned long uint64_t;
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

typedef uint64 pde_t;
typedef unsigned long  size_t;
typedef long           intptr_t;
typedef unsigned long  uintptr_t;