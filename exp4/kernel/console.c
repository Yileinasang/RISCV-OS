#include "def.h"
#define BACKSPACE 0x100

void console_putc(char c) {
  if(c == BACKSPACE){
    uart_putc('\b');
    uart_putc(' '); 
    uart_putc('\b');
  } 
  else {
    uart_putc(c);
  }
}

void console_puts(const char *s) {
  while (*s) {
    console_putc(*s++);
  }
}