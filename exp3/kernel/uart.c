#include <stdint.h>

#define UART_BASE 0x10000000
#define UART_THR  (*(volatile uint8_t *)(UART_BASE + 0))  // 发送保持寄存器
#define UART_LSR  (*(volatile uint8_t *)(UART_BASE + 5))  // 线路状态寄存器
#define LSR_THRE  0x20                                    // THR 空

void uart_init(void) {
    
}

void uartputc(char c) {
    // 轮询等待可写
    while ((UART_LSR & LSR_THRE) == 0) { }
    UART_THR = (uint8_t)c;
}
