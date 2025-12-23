#include "riscv.h"
#include "def.h"

#define min(a, b) ((a) < (b) ? (a) : (b))

static void readsb(int dev, struct superblock *sb) {
    struct buf *bp;

    // 读取超级块所在扇区
    bp = bread(dev, 1);
    // 将磁盘的超级块复制到系统的超级块中
    memmove(sb, bp->data, sizeof(*sb));
    
    // 释放缓冲区
    brelse(bp);
}

void fsinit(int dev) {
    // 读取超级块
    readsb(dev, &sb);
    if (sb.magic != FSMAGIC)
        panic("invalid file system");
    initlog(dev, &sb);
    ireclaim(dev);
}

static unsigned int balloc(unsigned int dev) {
    int b, bi, m;
    struct buf *bp;

    bp = 0;
    for (b = 0; b < sb.size; b += BPB) {
        bp = bread(dev, BBLOCK(b, sb));
        // 遍历块位图
        for (bi = 0; bi < BPB && b + bi < sb.size; bi++) {
            // 计算掩码
            m = 1 << (bi % 8);
            if ((bp->data[bi / 8] & m) == 0) {  
                // 将块位设置为1表示已分配                        
                bp->data[bi / 8] |= m; 
                log_write(bp);
                brelse(bp);
                // 返回分配的块号
                return b + bi;
            }
        }
        brelse(bp);
    }
    printf("balloc: out of blocks\n");
    return 0;
}

static void bfree(int dev, unsigned int b) {
    struct buf *bp;
    int bi, m;

    bp = bread(dev, BBLOCK(b, sb));
    bi = b % BPB;
    m = 1 << (bi % 8);
    if ((bp->data[bi / 8] & m) == 0)
        panic("freeing free block");
    // 将块位设置为0表示已释放
    bp->data[bi / 8] &= ~m;
    log_write(bp);
    brelse(bp);
}

struct {
    struct spinlock lock;
    struct inode inode[NINODE];
} itable;

void iinit() {
    int i = 0;

    initlock(&itable.lock, "itable");
    for (i = 0; i < NINODE; i++) {
        initsleeplock(&itable.inode[i].lock, "inode");
    }
}

static struct inode *iget(unsigned int dev, unsigned int inum);

struct inode *ialloc(unsigned int dev, short type) {
    int inum;
    struct buf *bp;
    struct dinode *dip;

    for (inum = 1; inum < sb.ninodes; inum++) {
        // 找到包含inode的磁盘块并定位inode
        bp = bread(dev, IBLOCK(inum, sb));
        dip = (struct dinode *)bp->data + inum % IPB;
        // 找到未使用的inode
        if (dip->type == 0) {
            // 清空inode并初始化类型
            memset(dip, 0, sizeof(*dip));
            dip->type = type;
            log_write(bp);
            brelse(bp);
            return iget(dev, inum);
        }
        brelse(bp);
    }
    printf("ialloc: no inodes\n");
    return 0;
}

void iupdate(struct inode *ip) {
    struct buf *bp;
    struct dinode *dip;

    // 读取inode并更新内容
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode *)bp->data + ip->inum % IPB;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->nlink = ip->nlink;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    
    // 将修改inode的记录写入日志
    log_write(bp);
    brelse(bp);
}

static struct inode *iget(unsigned int dev, unsigned int inum) {
    struct inode *ip, *empty;

    acquire(&itable.lock);

    empty = 0;
    for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
        if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
            // inode在缓存中，增加引用计数并返回
            ip->ref++;
            release(&itable.lock);
            return ip;
        }
        if (empty == 0 && ip->ref == 0)
            // 记录第一个空闲inode
            empty = ip;
    }

    if (empty == 0)
        panic("iget: no inodes");

    // 初始化新的inode，并标记需要从磁盘读取
    ip = empty;
    ip->dev = dev;
    ip->inum = inum;
    ip->ref = 1;
    ip->valid = 0;
    release(&itable.lock);

    return ip;
}

struct inode *idup(struct inode *ip) {
    acquire(&itable.lock);
    // 增加引用计数
    ip->ref++;
    release(&itable.lock);
    return ip;
}

void ilock(struct inode *ip) {
    struct buf *bp;
    struct dinode *dip;

    // if (ip == 0 || ip->ref < 1)
    //     panic("ilock");
    if (ip == 0)
        panic("ilock: ip is null");
    if (ip->ref < 1)
        panic("ilock: ip ref < 1");

    // 获取inode的睡眠锁
    acquiresleep(&ip->lock);

    // inode无效，从磁盘读取
    if (ip->valid == 0) {
        bp = bread(ip->dev, IBLOCK(ip->inum, sb));
        dip = (struct dinode *)bp->data + ip->inum % IPB;
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->nlink = dip->nlink;
        ip->size = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        brelse(bp);
        // inode更改为有效
        ip->valid = 1;
        if (ip->type == 0)
            panic("ilock: no type");
    }
}

void iunlock(struct inode *ip) {
    if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("iunlock");

    // 释放inode的睡眠锁
    releasesleep(&ip->lock);
}

void iput(struct inode *ip) {
    acquire(&itable.lock);

    // 释放已分配但孤立的inode
    if (ip->ref == 1 && ip->valid && ip->nlink == 0) {
        acquiresleep(&ip->lock);
        release(&itable.lock);

        // 清理inode数据块
        itrunc(ip);
        ip->type = 0;
        // 更新磁盘上的inode
        iupdate(ip);
        ip->valid = 0;

        releasesleep(&ip->lock);
        acquire(&itable.lock);
    }

    // 减少引用计数
    ip->ref--;
    release(&itable.lock);
}

void iunlockput(struct inode *ip) {
    iunlock(ip);
    iput(ip);
}

void ireclaim(int dev) {
    for (int inum = 1; inum < sb.ninodes; inum++) {
        struct inode *ip = 0;
        struct buf *bp = bread(dev, IBLOCK(inum, sb));
        struct dinode *dip = (struct dinode *)bp->data + inum % IPB;

        // 检查inode是否已分配并且孤立
        if (dip->type != 0 && dip->nlink == 0) { 
            printf("ireclaim: orphaned inode %d\n", inum);
            // 获取内存中的inode
            ip = iget(dev, inum);
        }
        brelse(bp);

        // 清理孤立inode
        if (ip) {
            begin_op();
            ilock(ip);
            iunlock(ip);
            iput(ip);
            end_op();
        }
    }
}

static unsigned int bmap(struct inode *ip, unsigned int bn) {
    unsigned int addr, *a;
    struct buf *bp;

    // 逻辑块号在直接块范围内
    if (bn < NDIRECT) {
        if ((addr = ip->addrs[bn]) == 0) {
            // 未分配物理块，分配一个新的
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            // 更新inode地址
            ip->addrs[bn] = addr;
        }
        return addr;
    }
    bn -= NDIRECT;

    // 逻辑块号减去直接块范围后在间接块范围内
    if (bn < NINDIRECT) {
        if ((addr = ip->addrs[NDIRECT]) == 0) {
            // 分配一个新的间接块
            addr = balloc(ip->dev);
            if (addr == 0)
                return 0;
            ip->addrs[NDIRECT] = addr;
        }
        bp = bread(ip->dev, addr);
        a = (unsigned int *)bp->data;
        if ((addr = a[bn]) == 0) {
            // 间接块中的物理块未分配，分配一个新的
            addr = balloc(ip->dev);
            if (addr) {
                // 更新间接块地址并写入日志
                a[bn] = addr;
                log_write(bp);
            }
        }
        brelse(bp);
        return addr;
    }

    panic("bmap: out of range");
}

void itrunc(struct inode *ip) {
    int i, j;
    struct buf *bp;
    unsigned int *a;

    for (i = 0; i < NDIRECT; i++) {
        if (ip->addrs[i]) {
            // 释放直接数据块
            bfree(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT]) {
        bp = bread(ip->dev, ip->addrs[NDIRECT]);
        a = (unsigned int *)bp->data;
        for (j = 0; j < NINDIRECT; j++) {
            // 释放间接数据块
            if (a[j])
                bfree(ip->dev, a[j]);
        }
        brelse(bp);
        bfree(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    // 清空inode大小并更新磁盘
    ip->size = 0;
    iupdate(ip);
}

int readi(struct inode *ip, int user_dst, unsigned long dst, unsigned int off, unsigned int n) {
    unsigned int tot, m;
    struct buf *bp;

    // 检查和限制读取范围
    if (off > ip->size || off + n < off)
        return 0;
    if (off + n > ip->size)
        n = ip->size - off;

    for (tot = 0; tot < n; tot += m, off += m, dst += m) {
        // 获取数据块地址并读取数据块
        unsigned int addr = bmap(ip, off / BSIZE);
        if (addr == 0)
            break;
        bp = bread(ip->dev, addr);
        m = min(n - tot, BSIZE - off % BSIZE);
        if (user_dst) {
            // 不支持读取到用户空间
            panic("readi: user_dst not supported");
        }
        else {
            // 拷贝数据到内核空间
            for (unsigned int i = 0; i < m; i++)
                ((char *)dst)[i] = bp->data[off % BSIZE + i];
        }
        brelse(bp);
    }
    return tot;
}

int writei(struct inode *ip, int user_src, unsigned long src, unsigned int off, unsigned int n) {
    unsigned int tot, m;
    struct buf *bp;

    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > MAXFILE * BSIZE)
        return -1;

    for (tot = 0; tot < n; tot += m, off += m, src += m) {
        unsigned int addr = bmap(ip, off / BSIZE);
        if (addr == 0)
            break;
        bp = bread(ip->dev, addr);
        m = min(n - tot, BSIZE - off % BSIZE);
        if (user_src) {
            // 不支持从用户空间写入
            panic("writei: user_src not supported");
        }
        else {
            // 写入数据
            for (unsigned int i = 0; i < m; i++)
                bp->data[off % BSIZE + i] = ((char *)src)[i];
        }
        // 将修改记录添加到日志中
        log_write(bp);
        brelse(bp);
    }

    // 更新文件大小
    if (off > ip->size)
        ip->size = off;

    // 更新磁盘中的inode
    iupdate(ip);

    return tot;
}

int namecmp(const char *s, const char *t) {
    return strncmp(s, t, DIRSIZ);
}

struct inode *dirlookup(struct inode *dp, char *name, unsigned int *poff) {
    unsigned int off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
        // 父目录inode不是目录，返回
        panic("dirlookup not DIR");

    for (off = 0; off < dp->size; off += sizeof(de)) {
        // 检查读取的目录项大小是否正确
        if (readi(dp, 0, (unsigned long)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup read");
        // 跳过未使用的目录项
        if (de.inum == 0)
            continue;
        if (namecmp(name, de.name) == 0) {
            // 名称匹配，更新偏移量
            if (poff)
                *poff = off;
            inum = de.inum;
            // 返回inode
            return iget(dp->dev, inum);
        }
    }

    return 0;
}

int dirlink(struct inode *dp, char *name, unsigned int inum) {
    int off;
    struct dirent de;
    struct inode *ip;

    if ((ip = dirlookup(dp, name, 0)) != 0) {
        // 目录中已存在同名项，返回
        iput(ip);
        return -1;
    }

    for (off = 0; off < dp->size; off += sizeof(de)) {
        if (readi(dp, 0, (unsigned long)&de, off, sizeof(de)) != sizeof(de))
            panic("dirlink read");
        // 找到第一个未使用的目录项
        if (de.inum == 0)
            break;
    }

    // 设置目录项名称和编号并写入inode文件
    strncpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (writei(dp, 0, (unsigned long)&de, off, sizeof(de)) != sizeof(de))
        return -1;

    return 0;
}

static char *skipelem(char *path, char *name) {
    char *s;
    int len;

    while (*path == '/')
        path++;
    // 跳过'/'，剩余路径为空则返回
    if (*path == 0)
        return 0;
    
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    // 复制下一级路径
    if (len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else {
        memmove(name, s, len);
        // 末尾加一个0表示截断
        name[len] = 0;
    }

    // 跳过剩余的'/'
    while (*path == '/')
        path++;
    return path;
}

static struct inode *namex(char *path, int nameiparent, char *name) {
    struct inode *ip, *next;

    if (*path == '/')
        // 绝对路径，从根目录开始
        ip = iget(ROOTDEV, ROOTINO);
    else
        // 相对路径，从当前目录开始
        ip = idup(current_proc->cwd);

    // 逐级解析路径
    while ((path = skipelem(path, name)) != 0) {
        ilock(ip);
        if (ip->type != T_DIR) {
            // 如果inode不是目录，返回
            iunlockput(ip);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // 返回最后一级目录的inode
            iunlock(ip);
            return ip;
        }
        if ((next = dirlookup(ip, name, 0)) == 0) {
            // 在新路径中查找下一级inode
            iunlockput(ip);
            return 0;
        }
        iunlockput(ip);
        ip = next;

    }
    if (nameiparent) {
        iput(ip);
        return 0;
    }
    return ip;
}

struct inode *namei(char *path) {
    char name[DIRSIZ];
    return namex(path, 0, name);
}

struct inode *nameiparent(char *path, char *name) {
    return namex(path, 1, name);
}
