# 实验报告

## 实验一：RISC-V引导与裸机启动



#### 系统设计说明
 **系统架构**  
CPU 上电 → `_start` (entry.S) → 设置栈 → 清零 BSS → 跳转 `kernel_main` → 调用 `uart_puts` 输出字符串 → 死循环等待  
 **模块划分**  
 `entry.S`：启动入口，初始化栈和 BSS  
 `kernel.ld`：内存布局，定义栈和段地址  
 `uart.c`：UART 驱动，提供 `uart_putc`、`uart_puts`  
 `main.c`：内核入口，调用 UART 输出  
 **核心模块设计**  
 UART 驱动：轮询 LSR 的 THRE 位，确保发送缓冲区空，再写 THR  
 **设计决策说明**  
 栈大小 16KB，放在内核镜像末尾  
 必须清零 BSS，保证全局变量符合 C 标准  
 使用 `-bios none`，完全裸机控制  

#### 实验过程说明
 **实验环境**：QEMU + riscv64-unknown-elf-gcc  
 **步骤与方案**：  
1. 编写 entry.S 设置栈和清零 BSS  
2. 编写 uart.c 驱动  
3. 编写 main.c 调用 uart_puts 输出  
4. 链接并运行  
 **遇到的问题**  
 QEMU 占用终端无法退出  
 输出乱码  
 **解决方案**  
 使用 `Ctrl+a x` 或 `pkill` 强制退出  
 确认波特率和寄存器地址，修正为 0x10000000  



###  实验测试结果
 **运行结果**
 **覆盖率**：验证了启动流程、栈初始化、BSS 清零、UART 输出  
 **性能测试**：简单字符串输出无性能瓶颈  
 **运行截图**：QEMU 控制台显示 "Hello OS"  


