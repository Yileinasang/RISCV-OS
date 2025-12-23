#include "def.h"
#include "virtio.h"
#include "riscv.h"

#define R(r) ((volatile unsigned int *)(VIRTIO0 + (r)))

static struct disk {
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
    char free[NUM];
    unsigned short used_idx;
    struct {
        struct buf *b;
        char status;
    } info[NUM];
    struct virtio_blk_req ops[NUM];
    struct spinlock vdisk_lock;
} disk;

void virtio_disk_init(void) {
    unsigned int status = 0;

    initlock(&disk.vdisk_lock, "virtio_disk");

    // 检查虚拟磁盘是否存在
    if (*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 || *R(VIRTIO_MMIO_VERSION) !=2 || *R(VIRTIO_MMIO_DEVICE_ID) != 2 || *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
        panic("could not find virtio disk");
    }

    // 设置设备状态
    *R(VIRTIO_MMIO_STATUS) = status;

    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;

    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;

    // 协商设备特性
    unsigned long features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;

    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    status = *R(VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_CONFIG_S_FEATURES_OK))
        panic("virtio disk FEATURES_OK unset");

    // 初始化队列，选择队列0
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;

    if (*R(VIRTIO_MMIO_QUEUE_READY))
        panic("virtio disk should not be ready");

    unsigned int max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if (max == 0)
        panic("virtio disk has no queue 0");
    if (max < NUM)
        panic("virtio disk max queue too short");

    // 分配描述符表、可用环和已用环，全部初始化为0
    disk.desc = alloc_page();
    disk.avail = alloc_page();
    disk.used = alloc_page();
    if (!disk.desc || !disk.avail || !disk.used)
        panic("virtio disk alloc_page");
    memset(disk.desc, 0, PGSIZE);
    memset(disk.avail, 0, PGSIZE);
    memset(disk.used, 0, PGSIZE);

    // 设置队列寄存器
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;

    *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (unsigned long)disk.desc;
    *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (unsigned long)disk.desc >> 32;
    *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (unsigned long)disk.avail;
    *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (unsigned long)disk.avail >> 32;
    *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (unsigned long)disk.used;
    *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (unsigned long)disk.used >> 32;

    *R(VIRTIO_MMIO_QUEUE_READY) = 0x1;

    // 初始化描述符状态
    for (int i = 0; i < NUM; i++)
        disk.free[i] = 1;

    // 通知设备初始化完成
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;

    register_interrupt(IRQ_VIRTIO, virtio_disk_intr);
}

static int alloc_desc() {
    for (int i = 0; i < NUM; i++) {
        if (disk.free[i]) {
            // 将空闲的描述符标记为已分配
            disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

static void free_desc(int i) {
    if (i >= NUM)
        panic("free_desc 1");
    if (disk.free[i])
        panic("free_desc 2");
    
    // 清空描述符内容并标记为空闲
    disk.desc[i].addr = 0;
    disk.desc[i].len = 0;
    disk.desc[i].flags = 0;
    disk.desc[i].next = 0;
    disk.free[i] = 1;

    // 唤醒等待空闲描述符的进程
    wakeup(&disk.free[0]);
}

static void free_chain(int i) {
    while (1) {
        // 释放描述符链
        int flag = disk.desc[i].flags;
        int nxt = disk.desc[i].next;
        free_desc(i);
        if (flag & VRING_DESC_F_NEXT)
            i = nxt;
        else
            break;
    }
}

static int alloc3_desc(int *idx) {
    for (int i = 0; i < 3; i++) {
        // 连续分配3个描述符
        idx[i] = alloc_desc();
        
        if (idx[i] < 0) {
            // 如果分配失败，清空已分配的描述符
            for (int j = 0; j < i; j++)
                free_desc(idx[j]);
            return -1;
        }
    }
    return 0;
}

void virtio_disk_rw(struct buf *b, int write) {
    // 计算扇区号
    unsigned long sector = b->blockno * (BSIZE / 512);

    // 获取磁盘的锁
    acquire(&disk.vdisk_lock);

    int idx[3];
    while (1) {
        // 分配3个描述符
        if (alloc3_desc(idx) == 0) {
            break;
        }
        // 分配失败，进入睡眠等待描述符释放
        sleep(&disk.free[0], &disk.vdisk_lock);
    }

    struct virtio_blk_req *buf0 = &disk.ops[idx[0]];

    if (write)
        buf0->type = VIRTIO_BLK_T_OUT;
    else
        buf0->type = VIRTIO_BLK_T_IN;
    buf0->reserved = 0;
    buf0->sector = sector;

    // 设置请求头描述符
    disk.desc[idx[0]].addr = (unsigned long)buf0;
    disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];

    // 设置数据描述符
    disk.desc[idx[1]].addr = (unsigned long)b->data;
    disk.desc[idx[1]].len = BSIZE;
    if (write)
        disk.desc[idx[1]].flags = 0;
    else
        disk.desc[idx[1]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];

    // 设置状态描述符
    disk.info[idx[0]].status = 0xff;
    disk.desc[idx[2]].addr = (unsigned long)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
    disk.desc[idx[2]].next = 0;

    b->disk = 1;
    disk.info[idx[0]].b = b;

    // 通知设备处理读写请求
    disk.avail->ring[disk.avail->idx % NUM] = idx[0];

    __sync_synchronize();

    disk.avail->idx += 1;

    __sync_synchronize();

    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;
    
    // 等待请求处理完毕
    while (__sync_synchronize(), b->disk == 1) {}

    // 清理描述符链
    disk.info[idx[0]].b = 0;
    free_chain(idx[0]);

    // 释放磁盘的锁
    release(&disk.vdisk_lock);
}

void virtio_disk_intr() {
    // 读取中断原因并写入中断确认寄存器
    *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

    __sync_synchronize();

    // 索引相等说明所有请求处理完毕
    while (disk.used_idx != disk.used->idx) {
        __sync_synchronize();
        int id = disk.used->ring[disk.used_idx % NUM].id;

        if (disk.info[id].status != 0)
            panic("virtio_disk_intr status");

        // 清零请求位
        struct buf *b = disk.info[id].b;
        b->disk = 0;
        disk.used_idx += 1;
    }
}
