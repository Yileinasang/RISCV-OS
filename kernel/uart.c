#include <stdint.h>

#define UART_BASE  ((volatile uint8_t *)0x10000000)
#define UART_THR   (UART_BASE + 0)   // 发送保持寄存器
#define UART_LSR   (UART_BASE + 5)   // 线路状态寄存器
#define LSR_THRE   0x20              // THR 空，允许写入

static inline void uart_putc(char c) {
    // 轮询等待 THR 空
    while ((*(UART_LSR) & LSR_THRE) == 0) { }
    *UART_THR = (uint8_t)c;
}

void uart_puts(const char *s) {
    for (; *s; s++) {
        if (*s == '\n') uart_putc('\r'); // 兼容终端换行
        uart_putc(*s);
    }
}


void uart_init(void) {
    
}
