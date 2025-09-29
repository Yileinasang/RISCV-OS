#include <stdint.h>

void uartputc(char c);

// 控制层：负责终端兼容（CRLF）、可能的缓冲与策略
void consputc(char c) {
    if (c == '\n')
        uartputc('\r');   // 兼容 nographic 终端换行
    uartputc(c);
}

void console_puts(const char *s) {
    while (*s) {
        consputc(*s++);
    }
}
