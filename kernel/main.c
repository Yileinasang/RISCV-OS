#include <stdint.h>

void uart_init(void);
void uart_puts(const char *s);

void kernel_main(void) {
    uart_init();

    uart_puts("Hello OS\n");

    for (;;) {
        __asm__ volatile ("wfi");
    }
}
