#include "def.h"

#define THR   0x00
#define LSR   0x05
#define LSR_TX_IDLE (1<<5)

#define Reg(reg) ((volatile unsigned char *)(UART0 + (reg)))
#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

void uart_putc(char c) {
    while((ReadReg(LSR) & LSR_TX_IDLE) == 0);
    WriteReg(THR, c);
}

void uart_puts(char *s) {
    while(*s) { uart_putc(*s++); }
}
