## 运行环境
qemu-riscv64

## 运行方式
在 `start.c` 文件中，找到 `main` 函数，修改以下代码中的数字以选择运行的实验：

```c
void main() {
    test_lab(8);
}
```

修改完成后，在终端中执行以下命令启动程序：
```bash
cd kernel && make run
```