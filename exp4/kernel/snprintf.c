#include <stdarg.h>
#include "def.h"
void itoa(int num, char *buf, int base) {
    int i = 0;
    int is_negative = 0;

    if (num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }

    do {
        int digit = num % base;
        buf[i++] = (digit < 10) ? ('0' + digit) : ('a' + (digit - 10));
        num /= base;
    } while (num > 0);

    if (is_negative) {
        buf[i++] = '-';
    }

    buf[i] = '\0';

    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = buf[j];
        buf[j] = buf[k];
        buf[k] = temp;
    }
}

int vsnprintf(char *str, unsigned int size, const char *format, va_list args) {
    int written = 0;
    const char *p = format;
    char *buf = str;

    while (*p && written < size - 1) {
        if (*p == '%') {
            p++;
            if (*p == 'd') {
                int num = va_arg(args, int);
                char numbuf[16];
                itoa(num, numbuf, 10);
                for (char *n = numbuf; *n && written < size - 1; n++) {
                    *buf++ = *n;
                    written++;
                }
            } else if (*p == 's') {
                char *s = va_arg(args, char *);
                while (*s && written < size - 1) {
                    *buf++ = *s++;
                    written++;
                }
            }
        } else {
            *buf++ = *p;
            written++;
        }
        p++;
    }

    *buf = '\0';
    return written;
}

int snprintf(char *str, unsigned int size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int n = vsnprintf(str, size, format, args);
    va_end(args);
    return n;
}