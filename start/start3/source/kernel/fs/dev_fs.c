#include "fs/dev_fs.h"
#include "fs/fs.h"
#include "dev/dev.h"
#include "tools/klib.h"

// 定义所有支持的设备
static devfs_type_t devfs_types[]={
        {.name="tty",.dev_type=DEV_MAJOR_TTY,.file_type=FILE_TTY},
};
static int devfs_type_size = sizeof(devfs_types)/ sizeof(devfs_type_t);

int devfs_mount(fs_t *fs,int major,int minor){
    fs->type=FS_DEV;
}

void devfs_unmount(fs_t *fs){

}

int devfs_open(fs_t *fs,char *path,file_t *file){
    // 循环看那个设备能处理，暂时只有tty设备
    for (int i = 0; i < devfs_type_size; ++i) {
        devfs_type_t *type = devfs_types+i;
        if(!kernel_strncmp(path,type->name,3)){
            continue;
        }
        file->dev_id= dev_open(type->dev_type,0,0);
        file->fs=fs;
        file->pos=0;
        file->size=0;
        file->type=type->file_type;
        return 0;
    }
    return 0;
}

int devfs_read(char *buf,int size,file_t *file){
    return dev_read(file->dev_id,file->pos,buf,size);
}

int devfs_write(char *buf,int size,file_t *file){
    return dev_write(file->dev_id,file->pos,buf,size);
}

void devfs_close(file_t *file){
    dev_close(file->dev_id);
}

int devfs_seek(file_t *file,uint32_t offset,int  dir){
    return -1;
}

int devfs_stat(file_t *file,struct stat *st){
    return -1;
}

fs_op_t devfs_op={
        .mount=devfs_mount,
        .unmount=devfs_unmount,
        .open=devfs_open,
        .read=devfs_read,
        .write=devfs_write,
        .close=devfs_close,
        .seek=devfs_seek,
        .stat=devfs_stat,
};
