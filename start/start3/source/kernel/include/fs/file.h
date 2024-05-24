#ifndef FILE_H
#define FILE_H

#include "comm/types.h"

#define FILE_NAME_SIZE 32
#define FILE_NUM 1024

#define FILE_UNKNOWN 0
#define FILE_TTY 1
#define FILE_DIR 2
#define FILE_NORMAL 3


typedef struct fs fs_t;

typedef struct file{
    char name[FILE_NAME_SIZE];
    uint8_t type;
    uint32_t size;
    int ref; // 引用次数
    int dev_id;
    int pos;
    int mode;
    int index; // fat16文件中对应的表头项
    int sblock; // 所处的簇号
    int cblock;
    fs_t *fs;
}file_t;

void file_init();
file_t *file_alloc();
void file_free(file_t *file);

typedef struct dir_item{
    int index;
    int type;
    char name[16];
    uint32_t size;
}dir_item_t;

typedef struct dir{
    int index;
    uint8_t use;
    dir_item_t item;
}dir_t;

#endif