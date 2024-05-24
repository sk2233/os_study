#ifndef FAT_FS_H
#define FAT_FS_H

#include "comm/types.h"
#include "comm/boot_info.h"
#include "file.h"

#pragma pack(1)
typedef struct dbr{
    uint8_t unused1[3];
    // 制造商名称
    uint8_t oem_name[8];
    // 一个扇区多少字节
    uint16_t sector_size;
    // 每簇的扇区数
    uint8_t cluster_count;
    // fat表项前多少扇区
    uint16_t reserve_size;
    // fat表的数量 2
    uint8_t fat_count;
    // 根目录标项数量
    uint16_t root_count;
    // 总计扇区数量
    uint16_t total_sector;
    // 设备类型 u盘等
    uint8_t media_type;
    // 一个fat表占用的扇区数
    uint16_t fat_size;
    uint8_t unused2[30];
    // 文件系统名称
    uint8_t sys_type[8];
}dbr_t;
#pragma pack()

typedef struct fs fs_t;

typedef struct fat{
    // fat表开始位置 只考虑第一个 fat表
    uint32_t fat_start;
    // fat表数量  一般是2个
    uint32_t fat_count;
    // fat占用多少字节
    uint32_t fat_size;
    // 一个簇多少扇区
    uint32_t cluster_count;
    // 一个簇多少字节
    uint32_t cluster_size;
    // 根目录描述区开始地址
    uint32_t root_start;
    // 根目录描述文件表项数目
    uint32_t root_count;
    // 数据开始地址
    uint32_t data_start;
    fs_t *fs;
    char buff[4096]; // 读取数据需要使用的缓存区 一次读取一个扇区 还有其他用处大一点
    int current_sector;
}fat_t;

void sys_ls(char *path);
void sys_unlink(char *path);

// 空闲表项
#define FAT_FREE 0
// 文件结束标记
#define FAT_END 0xFFFF

#endif