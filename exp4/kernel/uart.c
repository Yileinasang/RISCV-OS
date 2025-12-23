#include "def.h"

#define THR   0x00          // 发送寄存器偏移
#define LSR   0x05          // 线路状态寄存器偏移
#define LSR_TX_IDLE (1<<5)  // LSR的第5位，发送缓冲区空

#define Reg(reg) ((volatile unsigned char *)(UART0 + (reg)))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

void uart_putc(char c) {
    // 等待发送缓冲区空
    while((ReadReg(LSR) & LSR_TX_IDLE) == 0);
    // 写入字符到发送寄存器
    WriteReg(THR, c);
}

void uart_puts(char *s) {
    while(*s) {
        uart_putc(*s++);
    }
}