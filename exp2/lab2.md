## 实验二：内核printf与清屏功能实现




#### 系统设计说明
 **系统架构**
 **模块划分**  
 `uart.c`：硬件层，直接操作 UART  
 `console.c`：控制层，处理换行兼容、封装输出  
 `printf.c`：格式化层，实现 `%d/%x/%s/%c/%p`  
 **核心模块设计**  
 `printint`：将整数转换为字符串，避免递归，处理 INT_MIN  
 `printf`：直接处理可变参数，支持多种格式，不拆分为 vprintf  
 `clear_screen`：输出 ANSI 转义序列 `\033[2J\033[H`  
 `goto_xy`：移动光标  
 `printf_color`：彩色输出  
 **设计决策说明**  
 为什么需要 console 层？→ 解耦硬件和格式化逻辑，便于扩展  
 为什么不递归？→ 避免栈消耗，保证裸机环境安全  
 如何处理 INT_MIN？→ 转为 long，再取负，避免溢出  
 如何处理 NULL 字符串？→ 输出 "(null)"  

#### 实验过程说明
 **实验环境**：QEMU + riscv64-unknown-elf-gcc  
 **步骤与方案**：  
1. 在实验一基础上新增 console.c  
2. 实现 printf.c，支持基本格式化  
3. 扩展清屏、光标移动、彩色输出  
4. 在 main.c 测试  
 **遇到的问题**  
 `clear_screen` 在 VSCode 终端无效  
 链接错误：`undefined reference to clear_screen`  
 **解决方案**  
 换用 Windows Terminal / Linux 原生终端，确认 ANSI 支持  
 将 `clear_screen` 定义在全局作用域，避免写在函数内部  



###  实验测试结果
 **运行结果**
 **覆盖率**：验证了整数、字符串、指针、字符、清屏、光标移动、彩色输出  
 **性能测试**：大量字符串输出正常  
 **运行截图**：QEMU 控制台显示格式化输出和彩色效果（在支持 ANSI 的终端下）  


