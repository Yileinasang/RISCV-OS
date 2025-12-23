#include <stdint.h>

#define UART_BASE  ((volatile uint8_t *)0x10000000)
#define UART_RBR   (UART_BASE + 0)   // 接收缓冲寄存器（DLAB=0）
#define UART_THR   (UART_BASE + 0)   // 发送保持寄存器（DLAB=0）
#define UART_DLL   (UART_BASE + 0)   // 除数锁存低字节（DLAB=1）
#define UART_DLM   (UART_BASE + 1)   // 除数锁存高字节（DLAB=1）
#define UART_IER   (UART_BASE + 1)   // 中断使能（DLAB=0）
#define UART_FCR   (UART_BASE + 2)   // FIFO 控制
#define UART_LCR   (UART_BASE + 3)   // 线路控制
#define UART_MCR   (UART_BASE + 4)   // 调制解调控制
#define UART_LSR   (UART_BASE + 5)   // 线路状态

#define LCR_DLAB   0x80              // 允许访问 DLL/DLM
#define LCR_8N1    0x03              // 8 数据位，无校验，1 停止位
#define FCR_ENABLE 0x01              // FIFO 使能
#define FCR_CLEAR  0x06              // 清空 RX/TX FIFO
#define MCR_DTR_RTS 0x03             // 置 DTR/RTS
#define MCR_OUT2   0x08              // 允许中断线路

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
    // 关闭中断
    *UART_IER = 0x00;

    // 进入除数锁存模式，设置波特率：DLAB=1
    *UART_LCR = LCR_DLAB;
    // QEMU virt 默认 UART 输入时钟 3.6864MHz，除数 3 对应 38400 波特率
    *UART_DLL = 0x03;
    *UART_DLM = 0x00;

    // 退出 DLAB，配置 8N1
    *UART_LCR = LCR_8N1;

    // 打开并清空 FIFO
    *UART_FCR = FCR_ENABLE | FCR_CLEAR;

    // 设置调制控制：置 DTR/RTS，并打开 OUT2 以允许中断线路
    *UART_MCR = MCR_DTR_RTS | MCR_OUT2;
}
