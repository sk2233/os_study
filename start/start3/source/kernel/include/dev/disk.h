#ifndef DISK_H
#define DISK_H
#include "comm/types.h"
#include "cpu/irq.h"

#define PART_NAME_SIZE 32

#define PART_TYPE_INVALID 0x00
#define PART_TYPE_FAT16A  0x6
#define PART_TYPE_FAT16B  0xE

typedef struct disk disk_t;

typedef struct part{
    char name [PART_NAME_SIZE];
    disk_t *disk;
    int type;
    uint32_t start_sector;
    uint32_t total_sector;
}part_t;

#define DISK_NAME_SIZE 32
// 一般为4，这里特殊设置为5
#define DISK_PART_SIZE 5

#define DISK_COUNT 2

// 主从盘标记 方便后面位运算
#define DISK_DRIVE_MASTER (0<<4)
#define DISK_DRIVE_SLAVE (1<<4)

// 硬盘端口
#define DISK_DATA(disk) (disk->port_base+0)
#define DISK_ERR(disk) (disk->port_base+1)
#define DISK_SECTOR_COUNT(disk) (disk->port_base+2)
#define DISK_LBA_LO(disk) (disk->port_base+3)
#define DISK_LBA_MI(disk) (disk->port_base+4)
#define DISK_LBA_HI(disk) (disk->port_base+5)
#define DISK_DRIVE(disk) (disk->port_base+6)
#define DISK_STAT_CMD(disk) (disk->port_base+7)

// 读取的状态码
#define DISK_STAT_ERR (1<<0)
#define DISK_STAT_DRQ (1<<3)
#define DISK_STAT_DF (1<<5)
#define DISK_STAT_BUSY (1<<7)

// 对硬盘下达的命令
#define DISK_CMD_IDENT 0xEC
#define DISK_CMD_READ 0x24
#define DISK_CMD_WRITE 0x34

struct disk{
    char name[DISK_NAME_SIZE];
    int sector_size; // 扇区的大小一般 512
    int sector_count;
    int drive; // 主盘还是从盘，没有复制关系的
    int port_base; // 数据端口偏移，每个硬盘数据端口是不一样的，但是相对值相同    0x1f0
    part_t parts[DISK_PART_SIZE];
};

// 16B
#pragma pack(1)
typedef struct part_item{
    uint8_t active; // 是否激活 可用
    // 开始位置 使用lba模式 不使用这些参数
    uint8_t start_header;
    uint16_t start_sector:6;
    uint16_t start_cylinder:10;
    // 文件系统类型
    uint8_t system_id;
    // 结束位置 使用lba模式 不使用这些参数
    uint8_t end_header;
    uint16_t end_sector:6;
    uint16_t end_cylinder:10;
    // 这个才是需要关注的 开始扇区偏移 与总扇区数
    uint32_t relative_sector;
    uint32_t total_sector;
}part_item_t;

#define MBR_PART_COUNT 4

// 就是第一个扇区
typedef struct mbr{
    uint8_t code[446];
    // 实际只关心这部分
    part_item_t part_items[MBR_PART_COUNT];
    uint8_t boot_sign[2];
}mbr_t;
#pragma pack()

void disk_init();
void exception_handler_disk ();

#endif