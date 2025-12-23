#include <stdint.h>

#define UART_BASE 0x10000000
#define UART_RBR (*(volatile uint8_t *)(UART_BASE + 0))   // 接收缓冲（DLAB=0）
#define UART_THR (*(volatile uint8_t *)(UART_BASE + 0))   // 发送保持（DLAB=0）
#define UART_DLL (*(volatile uint8_t *)(UART_BASE + 0))   // 除数低字节（DLAB=1）
#define UART_DLM (*(volatile uint8_t *)(UART_BASE + 1))   // 除数高字节（DLAB=1）
#define UART_IER (*(volatile uint8_t *)(UART_BASE + 1))   // 中断使能
#define UART_FCR (*(volatile uint8_t *)(UART_BASE + 2))   // FIFO 控制
#define UART_LCR (*(volatile uint8_t *)(UART_BASE + 3))   // 线路控制
#define UART_MCR (*(volatile uint8_t *)(UART_BASE + 4))   // 调制控制
#define UART_LSR (*(volatile uint8_t *)(UART_BASE + 5))   // 线路状态

#define LCR_DLAB   0x80
#define LCR_8N1    0x03
#define FCR_ENABLE 0x01
#define FCR_CLEAR  0x06
#define MCR_DTR_RTS 0x03
#define MCR_OUT2   0x08
#define LSR_THRE   0x20                                    // THR 空

void uart_init(void) {
    // 关闭中断
    UART_IER = 0x00;

    // 进入除数锁存模式，配置波特率 (3.6864MHz / 3 = 38400)
    UART_LCR = LCR_DLAB;
    UART_DLL = 0x03;
    UART_DLM = 0x00;

    // 配置 8N1 并退出 DLAB
    UART_LCR = LCR_8N1;

    // 使能并清空 FIFO
    UART_FCR = FCR_ENABLE | FCR_CLEAR;

    // 置 DTR/RTS，打开 OUT2（常见 UART 需要）
    UART_MCR = MCR_DTR_RTS | MCR_OUT2;
}

void uartputc(char c) {
    // 轮询等待可写
    while ((UART_LSR & LSR_THRE) == 0) { }
    UART_THR = (uint8_t)c;
}
