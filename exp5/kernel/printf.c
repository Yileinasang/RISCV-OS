#include <stdarg.h>
#include "def.h"

void print_number(int num, int base, int sign) {
    char buf[20];
    int i = 0;
    unsigned int x;
    if (sign && num < 0) x = -num; else x = num;
    do {
        int digit = x % base;
        if (digit < 10) buf[i++] = '0' + digit; else buf[i++] = 'a' + (digit - 10);
        x /= base;
    } while (x != 0);
    if (sign && num < 0) { buf[i++] = '-'; }
    while (--i >= 0) { console_putc(buf[i]); }
}

void print_ptr(unsigned long x) {
    int i; console_putc('0'); console_putc('x');
    for (i = (sizeof(unsigned long) * 2) - 1; i >= 0; i--) {
        unsigned long digit = (x >> (i * 4)) & 0xF;
        if (digit < 10) console_putc('0' + digit); else console_putc('a' + (digit - 10));
    }
}

int printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { console_putc(*p); continue; }
        p++;
        switch (*p) {
            case 'd': { print_number(va_arg(ap, int), 10, 1); break; }
            case 'x': { print_number(va_arg(ap, unsigned int), 16, 0); break; }
            case 's': { char *str = va_arg(ap, char *); if (str == 0) str = "(null)"; console_puts(str); break; }
            case 'c': { char c = (char)va_arg(ap, int); console_putc(c); break; }
            case 'p': { print_ptr(va_arg(ap, unsigned long)); break; }
            case '%': { console_putc('%'); break; }
            default: { console_puts("Unknown code: %"); console_putc(*p); console_putc('\n'); break; }
        }
    }
    va_end(ap); return 0;
}

void panic(char *s) { console_puts("panic: "); console_puts(s); console_putc('\n'); while (1); }

void clear_screen(void) { console_puts("\033[2J"); console_puts("\033[H"); }
