
#include "def.h"
#include "riscv.h"

#define NDEV 10

static int isdirempty(struct inode *dp);

static int fdalloc(struct file *f) {
    int fd;
    struct proc *p = current_proc;

    for (fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd] == 0) {
            // 找到一个空闲的文件描述符，把文件赋给这个描述符来加入进程
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

int sys_open(char *path, int omode) {
    int fd;
    struct file *f;
    struct inode *ip;

    begin_op();

    if (omode & O_CREATE) {
        // 允许创建新文件
        ip = create(path, T_FILE, 0, 0);
        if (ip == 0) {
            end_op();
            return -1;
        }
    }
    else {
        // 不允许创建文件，查找已有文件
        if ((ip = namei(path)) == 0) {
            end_op();
            return -1;
        }
        ilock(ip);
        if (ip->type == T_DIR && omode != O_RDONLY) {
            // 不允许以非只读方式打开目录
            iunlockput(ip);
            end_op();
            return -1;
        }
    }

    if (ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)) {
        // 访问设备文件但设备号无效
        iunlockput(ip);
        end_op();
        return -1;
    }

    // 分配文件结构和文件描述符
    if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        iunlockput(ip);
        end_op();
        return -1;
    }

    // 设置文件基本信息
    if (ip->type == T_DEVICE) {
        f->type = FD_DEVICE;
        f->major = ip->major;
    }
    else {
        f->type = FD_INODE;
        f->off = 0;
    }
    f->ip = ip;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

    if ((omode & O_TRUNC) && ip->type == T_FILE) {
        // 截断文件
        itrunc(ip);
    }

    iunlock(ip);
    end_op();

    return fd;
}

struct inode *create(char *path, short type, short major, short minor) {
    struct inode *ip, *dp;
    char name[DIRSIZ];

    // 获取父目录
    if ((dp = nameiparent(path, name)) == 0)
        return 0;

    ilock(dp);

    if ((ip = dirlookup(dp, name, 0)) != 0) {
        // 文件存在于父目录下
        iunlockput(dp);
        ilock(ip);
        if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
            // 文件类型正确，返回inode
            return ip;
        iunlockput(ip);
        return 0;
    }

    // 文件不存在，创建新的inode
    if ((ip = ialloc(dp->dev, type)) == 0)
        panic("create: ialloc");

    // 初始化inode信息并写入磁盘
    ilock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    iupdate(ip);

    if (type == T_DIR) {
        // 类型为目录，更新父目录链接
        dp->nlink++;
        iupdate(dp);
        // 创建.和..链接
        if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
            panic("create dots");
    }

    // 创建父目录和新文件链接
    if (dirlink(dp, name, ip->inum) < 0)
        panic("create: dirlink");

    iunlockput(dp);

    return ip;
}

int sys_close(int fd) {
    struct file *f;
    struct proc *p = current_proc;

    if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -1;
    p->ofile[fd] = 0;
    fileclose(f);
    return 0;
}

int sys_read(int fd, char *buf, int n) {
    struct file *f;
    struct proc *p = current_proc;

    if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -1;
    return fileread(f, (unsigned long)buf, n);
}

int sys_write(int fd, char *buf, int n) {
    struct file *f;
    struct proc *p = current_proc;

    if (fd < 0 || fd >= NOFILE || (f = p->ofile[fd]) == 0)
        return -1;
    return filewrite(f, (unsigned long)buf, n);
}

int sys_unlink(char *path) {
    struct inode *ip, *dp;
    struct dirent de;
    char name[DIRSIZ];
    unsigned int off;

    begin_op();

    // 获取父目录
    if ((dp = nameiparent(path, name)) == 0) {
        end_op();
        return -1;
    }

    ilock(dp);

    // 不允许删除.和..
    if (namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
        iunlockput(dp);
        end_op();
        return -1;
    }

    // 查找目标文件的inode
    if ((ip = dirlookup(dp, name, &off)) == 0) {
        iunlockput(dp);
        end_op();
        return -1;
    }
    ilock(ip);

    // 检查inode是否有链接
    if (ip->nlink < 1)
        panic("unlink: nlink < 1");

    // 如果是目录，检查是否为空目录
    if (ip->type == T_DIR && !isdirempty(ip)) {
        iunlockput(ip);
        iunlockput(dp);
        end_op();
        return -1;
    }

    // 目录项的inode设置为0表示删除
    de.inum = 0;
    if (writei(dp, 0, (unsigned long)&de, off, sizeof(de)) != sizeof(de))
        panic("unlink: writei");
    
    // 如果是目录，减少父目录链接
    if (ip->type == T_DIR) {
        dp->nlink--;
        iupdate(dp);
    }
    iunlockput(dp);

    // 减少当前inode链接
    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);

    end_op();

    return 0;
}

static int isdirempty(struct inode *dp) {
    int off;
    struct dirent de;

    for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de)){
        if (readi(dp, 0, (unsigned long)&de, off, sizeof(de)) != sizeof(de))
            panic("isdirempty: readi");
        // 目录项inode编号不等于0表示目录不为空
        if (de.inum != 0)
            return 0;
    }
    return 1;
}