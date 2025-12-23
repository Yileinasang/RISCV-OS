#include <stdint.h>

void uart_init(void);
int printf(const char *fmt, ...);
void clear_screen(void);

// 测试用例：基础格式
static void test_printf_basic(void) {
    printf("Testing integer: %d\n", 42);
    printf("Testing negative: %d\n", -123);
    printf("Testing zero: %d\n", 0);
    printf("Testing hex: 0x%x\n", 0xABC);
    printf("Testing string: %s\n", "Hello");
    printf("Testing char: %c\n", 'X');
    printf("Testing percent: %%\n");
}

// 测试用例：边界情况
static void test_printf_edge_cases(void) {
    printf("INT_MAX: %d\n", 2147483647);
    printf("INT_MIN: %d\n", -2147483648);
    printf("NULL string: %s\n", (char*)0);
    printf("Empty string: %s\n", "");
}

void kernel_main(void) {
    uart_init();

    printf("Hello, OS\n");
    test_printf_basic();
    test_printf_edge_cases();

    for(volatile int i = 0; i < 100000000; i++);

    clear_screen();
    for (;;) {
        __asm__ volatile ("wfi");
    }
}
