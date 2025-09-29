#include <stdarg.h>
#include <stdint.h>

void consputc(char c);

// 打印整数（十进制/十六进制），最小实现，不用递归
static void printint(int num, int base, int sign) {
    char buf[32];
    int i = 0;
    unsigned int x;

    // 负数按无符号处理，避免递归与最小实现复杂度
    if (sign && num < 0) {
        // 处理负号并转为无符号
        x = (unsigned int)(-(long)num);  // 用 long 防止 -INT_MIN 溢出路径
    } else {
        x = (unsigned int)num;
    }

    // 转换数字
    do {
        int d = x % base;
        buf[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        x /= base;
    } while (x);

    // 负号
    if (sign && num < 0) {
        buf[i++] = '-';
    }

    // 反向输出
    while (--i >= 0) {
        consputc(buf[i]);
    }
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            consputc(*fmt);
            continue;
        }
        fmt++;
        if (*fmt == 0) break;

        switch (*fmt) {
        case 'd': {
            int val = va_arg(ap, int);
            printint(val, 10, 1);
            break;
        }
        case 'x': {
            int val = va_arg(ap, int);
            printint(val, 16, 0);
            break;
        }
        case 'p': { // 简化的指针打印（按 64 位十六进制）
            uint64_t v = (uint64_t)va_arg(ap, void *);
            consputc('0'); consputc('x');
            // 逐 nibble 打印
            for (int i = 60; i >= 0; i -= 4) {
                int d = (v >> i) & 0xF;
                consputc(d < 10 ? '0' + d : 'a' + d - 10);
            }
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s) consputc(*s++);
            break;
        }
        case 'c': {
            char c = (char)va_arg(ap, int);
            consputc(c);
            break;
        }
        case '%': {
            consputc('%');
            break;
        }
        default:
            // 未知格式，按原样输出，便于错误恢复
            consputc('%');
            consputc(*fmt);
            break;
        }
    }



    va_end(ap);
    return 0;
}

void clear_screen(void) {
    // ANSI 转义序列：清屏并把光标移到左上角
    printf("\033[2J\033[H");
    //printf("\033[H");
    //printf("[DEBUG] clear_screen called\n");
}