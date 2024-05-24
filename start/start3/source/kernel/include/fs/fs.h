/**
 * 文件系统相关接口的实现
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#ifndef FS_H
#define FS_H
#include "sys/stat.h"
#include "file.h"
#include "comm/types.h"
#include "tools/list.h"
#include "fs/fat_fs.h"
#include "applib/lib_syscall.h"

#define FS_DEV 1
#define FS_FAT16 2

typedef struct fs fs_t;

typedef struct fs_op{
    int (*mount)(fs_t *fs,int major,int minor);
    void (*unmount)(fs_t *fs);
    int (*open)(fs_t *fs,char *path,file_t *file);
    int (*read)(char *buf,int size,file_t *file);
    int (*write)(char *buf,int size,file_t *file);
    void (*close)(file_t *file);
    int (*seek)(file_t *file,uint32_t offset,int  dir);
    int (*stat)(file_t *file,struct stat *st);
}fs_op_t;

struct fs{
    char mount_point[32];
    int type;
    fs_op_t *op;
    void *data;
    int dev_id;
    list_node_t node;
    fat_t fat;
};

int sys_open(char *name, int flags, ...);
int sys_read(int fd, char *ptr, int len);
int sys_write(int file, char *ptr, int len);
int sys_lseek(int file, int ptr, int dir);
int sys_close(int file);

int sys_isatty(int file);
int sys_fstat(int file,struct stat *st);

void fs_init();
fs_t *get_root_fs();

#define NAME_EMPTY 0xE5
#define NAME_END 0x00

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_DIRECTORY 0x10

// 32byte
typedef struct file_item{
    uint8_t name[11];
    uint8_t attr;
    uint8_t unused1[8];
    uint16_t clu_hi;
    uint8_t unused2[4];
    uint16_t clu_lo;
    uint32_t size;
}file_item_t;

#endif // FILE_H

