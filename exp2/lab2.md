# 实验报告（exp2：内核 printf 与清屏）

## 实验概述
- 实验目标：实现内核级 `printf` 与 `clear_screen`，完成基本格式化输出与串口显示。
- 完成情况：`printf/printint/consputc/uartputc` 正常，ANSI 清屏可用，QEMU 串口输出通过。
- 开发环境：Ubuntu 22.04；riscv64-unknown-elf-gcc 12.x；qemu-system-riscv64 7.x。

## 关键函数实现
- 格式化核心（`kernel/printf.c`）：有限状态机解析 `%d/%x/%p/%s/%c/%%`，整数用迭代除基数组装。
```c
static void printint(long xx,int base,int sign){
  char buf[32];int i=0;ulong x=sign&&xx<0?-xx:xx;
  do{buf[i++]=digits[x%base];x/=base;}while(x!=0);
  if(sign&&xx<0)buf[i++]='-';
  while(--i>=0)consputc(buf[i]);
}
```
- 控制台输出（`kernel/console.c`）：处理 CRLF 兼容后写 UART。
```c
void consputc(int c){ if(c=='\n') uartputc('\r'); uartputc(c);} 
```
- 清屏（`kernel/printf.c`）：发送 ANSI 转义。
```c
void clear_screen(void){ printf("\033[2J\033[H"); }
```

## 难点突破
- CRLF 兼容：在 `consputc` 对 `\n` 注入 `\r`，适配 nographic 终端。
- 指针打印：`%p` 固定 16 hex 宽度用于调试地址。

## 源码理解与对比
- 相同：最小 printf、串口轮询输出与 xv6 接近。
- 不同：未加锁（单核场景足够）；清屏走 ANSI 而非显存操作。

## 测试情况
- 基本：`make && make run`，查看格式化输出与清屏效果。
- 边界：为空指针/长字符串/未知格式符验证容错与恢复。

## 思考题与回答
- 架构设计：
  - 分层必要：驱动/控制台/格式层解耦，方便扩展与测试。
  - 多设备：抽象 `console` 设备表，选择路由或广播输出。
- 算法选择：
  - 非递归：栈有限；迭代更稳定。
  - 无除法转换：2/8/16 用位运算；10 进制用 double-dabble/查表。
- 性能优化：
  - 瓶颈：MMIO 延迟与解析开销。
  - 缓冲：环形缓冲 + 批量发送，减少锁竞争。
- 错误处理：
  - `NULL`：打印 `(null)`；
  - 格式错误：原样回退并记录警告。
