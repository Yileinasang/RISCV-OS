# 实验报告（exp7：文件系统）

## 实验概述
- 实验目标：实现带写前日志的简化文件系统，支持创建/读写/删除、并发访问与崩溃恢复。
- 完成情况：`bio/fs/file/log/virtio` 模块齐全，`fsinit` 恢复日志正常；并发/性能/崩溃恢复自测通过。
- 开发环境：Ubuntu 22.04；riscv64-unknown-elf-gcc 12.x；qemu-system-riscv64 7.x。

## 关键函数实现
- 日志提交（`kernel/log.c`）：防溢出并按顺序提交。
```c
void end_op(void){ log.outstanding--; if(log.committing) panic("log busy"); if(log.outstanding==0){ log.committing=1; write_log(); write_head(); install_trans(0); log.lh.n=0; write_head(); log.committing=0; wakeup(&log);} }
```
- 块缓存（`kernel/bio.c`）：LRU 获取并加 pin。
```c
struct buf* bget(uint dev,uint blockno){ acquire(&bcache.lock); struct buf *b=find_buf(dev,blockno); if(!b) b=evict_lru(); b->dev=dev; b->blockno=blockno; b->refcnt=1; release(&bcache.lock); acquiresleep(&b->lock); return b; }
```
- inode 读写（`kernel/fs.c`）：目录查找与数据块分配。
```c
int readi(struct inode *ip,int user,uint64 dst,uint off,uint n){ for(uint tot=0; tot<n; tot+=m){ bp=bread(ip->dev,bmap(ip,off/BSIZE)); m=min(n-tot,BSIZE-off%BSIZE); either_copyout(user,dst,bp->data+off%BSIZE,m); brelse(bp);} }
```

## 难点突破
- 日志溢出：`begin_op` 依据 `MAXOPBLOCKS` 与 outstanding 上限阻塞，拆分大写入避免超限。
- 并发一致性：睡眠锁保护缓存与 inode；避免持锁 `yield` 导致死锁。

## 源码理解与对比
- 相同：单事务日志 + 块缓存 + inode 结构与 xv6 一致。
- 不同：增加 `bcache_reset`/`ireclaim` 用于崩溃模拟与清理；并发测试覆盖更高。

## 测试情况
- 功能：`make && make run`，执行创建/读写/删除、并发小文件与 128KB 大文件测试。
- 崩溃恢复：`simulate_crash_reboot` 后再次挂载验证未提交写被回滚。
- 性能：统计提交次数与缓存命中率，瓶颈在小写放大与目录线性查找。

## 思考题与回答
- 设计权衡：
  - xv6 FS 优缺点：简单易实现；性能与扩展性有限（线性目录、单事务）。
  - 平衡：保持接口简单同时用缓冲、预读与日志批处理提升性能。
- 一致性保证：
  - 原子性：写前日志记录修改块，提交头为屏障；安装事务后清空头确保幂等。
  - 恢复再次崩溃：恢复过程幂等，重复执行不损坏数据，但需保证头写入顺序。
- 性能优化：
  - 瓶颈：小写放大、目录线性扫描、同步写等待。
  - 查找效率：缓存目录项、哈希索引或 B-tree。
- 可扩展性：
  - 更大文件：双重间接或 extent 映射；扩展超级块字段。
  - 先进特性：延迟分配、事务组、写时复制、校验与压缩。
- 可靠性：
  - 检测与修复：离线 fsck；校验 inode/块引用关系。
  - 在线检查：后台巡检、只读挂载、日志一致性快速校验。
