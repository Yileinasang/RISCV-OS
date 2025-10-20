#include "types.h"

extern void printf(const char*, ...);

void panic(char *s) {
    printf("panic: %s\n", s);
    for(;;); // 死循环
}
