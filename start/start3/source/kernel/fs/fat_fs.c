#include <stdbool.h>
#include "fs/fat_fs.h"
#include "dev/dev.h"
#include "comm/boot_info.h"
#include "fs/fs.h"
#include "tools/klib.h"
#include "sys/fcntl.h"
#include "tools/log.h"

static dbr_t dbr; // 公用的临时存储

int fatfs_mount(fs_t *fs,int major,int minor){
    // 打开读取的是fat设备 是有基础偏移的
    int dev_id=dev_open(major,minor,0);
    dev_read(dev_id,0,(char *)&dbr, 1);// 直接从0读取设置信息 以扇区为基本单位
    // 互相设置信息
    fat_t *fat=&fs->fat;
    fat->fat_start=dbr.reserve_size;
    fat->fat_count=dbr.fat_count; // 2
    fat->fat_size=dbr.fat_size;
    fat->cluster_count=dbr.cluster_count;
    fat->cluster_size=fat->cluster_count*SECTOR_SIZE;
    fat->root_start=fat->fat_start+fat->fat_size*fat->fat_count;
    fat->root_count=dbr.root_count;
    fat->data_start=fat->root_start+fat->root_count*32/SECTOR_SIZE; // 为什么是除法?
    fat->fs=fs;
    fat->current_sector=-1;// 表示缓存无效
    fs->dev_id=dev_id;
    fs->type=FS_FAT16;
    return 0;
}

void fatfs_unmount(fs_t *fs){
    dev_close(fs->dev_id);
}

void to_short_name(char *buff, const char *path) {
    kernel_memset(buff,' ',11);
    int index =0;
    while (index<11){
        if(!*path){
            return;
        }
        if(*path=='.'){
            index=8; // 处理扩展名
            path++;
            continue;
        }
        buff[index]=(char )(*path-'a'+'A');// 转换为大写 没有处理数字注意!!!
        index++;
        path++;
    }
}

// name 8+3  不足使用空格
bool file_name_match(const char *path, const char *name) {
    char buff[11];
    to_short_name(buff,path);
    return kernel_strncmp(buff,name,11);
}

int file_item_type(uint8_t attr) {
    if((attr&0xF)==0xF){ // 改项目是长文件名 不作为正常文件头使用 跳过
        return FILE_UNKNOWN;
    }
    if((attr&ATTR_DIRECTORY)==ATTR_DIRECTORY){
        return FILE_DIR;
    }
    return FILE_NORMAL;
}

void read_from_item(file_t *file, file_item_t *item,int index) {
    file->type= file_item_type(item->attr);
    file->size=item->size;
    file->pos=0;
    file->index=index;
    file->sblock=(item->clu_hi<<16)|(item->clu_lo);
    file->cblock= file->sblock;
}

file_item_t *read_file_item(fat_t *fat, int index) {
    // 计算字节偏移
    int offset = index * (int )sizeof(file_item_t);
    // 计算所处的扇区 root_start 是开始的扇区号
    int sector = (int )fat->root_start+offset/SECTOR_SIZE;
    // 读取扇区 每次读取一个扇区并缓冲 若下次读同一个扇区就直接使用
    if(fat->current_sector!=sector){
        dev_read(fat->fs->dev_id,sector,fat->buff,1);
        fat->current_sector=sector;
    }
    // 一次读取一个扇区，读取到 buff 中 获取其偏移
    return (file_item_t *)(fat->buff+offset%SECTOR_SIZE);
}

int get_empty_index(fat_t *fat) {
    // 找一个空表项
    for (int i = 0; i < 100; ++i) {
        file_item_t *item=read_file_item(fat,i);
        if(item->name[0]==NAME_END){
            return i;
        }
    }
    return -1;
}

void file_item_init(file_item_t *item, int attr, char *name) {
    to_short_name((char *)item->name,name);
    // 设置为无效值 防止后续真的被使用
    item->clu_hi=0;
    item->clu_lo=FAT_END;
    item->size=0;
    item->attr=attr;
}

void write_file_item(fat_t *fat, file_item_t *item, int index) {
    // 计算字节偏移
    int offset = index * (int )sizeof(file_item_t);
    // 计算所处的扇区 root_start 是开始的扇区号
    int sector = (int )fat->root_start+offset/SECTOR_SIZE;
    // 先读取对应的扇区，再修改扇区，最后再写入扇区
    dev_read(fat->fs->dev_id,sector,fat->buff,1);
    fat->current_sector=sector;
    kernel_memcpy(fat->buff+offset%SECTOR_SIZE,item, sizeof(file_item_t));
    dev_write(fat->fs->dev_id,sector,fat->buff,1);
}

int fatfs_open(fs_t *fs, char *path, file_t *file){
    fat_t *fat=&fs->fat;
    // 会提前因为 NAME_END 而跳出
    for (int i = 0; i < 100; ++i) {
        file_item_t *item=read_file_item(fat,i);
        if(item->name[0]==NAME_EMPTY){
            continue;
        }
        if(item->name[0]==NAME_END){
            break;
        }
        if(!file_name_match(path,(char *)item->name)){
            continue;
        }
        read_from_item(file,item,i);
        return 0;
    }
    // 尝试创建文件
    if(file->mode&O_CREAT){
        int index = get_empty_index(fat);
        file_item_t item;
        // 初始化表项
        file_item_init(&item,0,path);
        // 写入表项
        write_file_item(fat,&item,index);
        read_from_item(file,&item,index);
        return 0;
    }
    return -1;
}

// block的移动不是相邻的需要查表
int get_next_block(fat_t *fat, int curr) {
    // 获取在第几个扇区内
    int offset = curr*2/SECTOR_SIZE;
    fat->current_sector=-1;
    dev_read(fat->fs->dev_id,(int )fat->fat_start+offset,fat->buff,1);
    offset=curr*2%SECTOR_SIZE; // 在扇区内的偏移
    return *(int16_t *)(fat->buff+offset);// 转换为对应的下标
}

void set_next_block(fat_t *fat, int block, int16_t next) {
    // 获取在第几个扇区内
    int offset = block*2/SECTOR_SIZE;
    fat->current_sector=-1;
    // 还是先读取，修改完再写入
    dev_read(fat->fs->dev_id,(int )fat->fat_start+offset,fat->buff,1);
    offset=block*2%SECTOR_SIZE; // 在扇区内的偏移
    *(int16_t *)(fat->buff+offset)=next;
    dev_write(fat->fs->dev_id,(int )fat->fat_start+offset,fat->buff,1);
}

int fatfs_read(char *buf, int size, file_t *file){
    // 设置最大读取数量
    uint32_t need_read = size;
    if(file->pos+need_read>file->size){
        need_read=file->size-file->pos;
    }

    fs_t *fs=file->fs;
    fat_t *fat = &fs->fat;
    uint32_t end_pos = file->pos+need_read;
    while (file->pos<end_pos){
        // 获取当前位置在 簇内的偏移
        uint32_t offset = file->pos %fat->cluster_size;
        // 注意最开始的簇是2 从2开始 获取簇号计算扇区号
        uint32_t sector = fat->data_start+(file->cblock-2)*fat->cluster_count;
        // 先读取到fat缓冲区中，并废弃缓冲标识
        dev_read(fs->dev_id,(int )sector,fat->buff, (int )fat->cluster_count);
        fat->current_sector=-1;
        if(offset==0){// 平整开始的
            if(end_pos>=file->pos+fat->cluster_size){// 直接读取一个block
                kernel_memcpy(buf,fat->buff,(int )fat->cluster_size);
                file->pos+=(int )fat->cluster_size;
                buf+=fat->cluster_size;
                file->cblock=get_next_block(fat,file->cblock);
            } else{// 不足一个block
                kernel_memcpy(buf,fat->buff,(int )end_pos-file->pos);
                file->pos=(int )end_pos;
            }
        } else{// 非平整开始
            if(end_pos>=file->pos+fat->cluster_size){// 直接读取一个block
                kernel_memcpy(buf,fat->buff+offset,(int )(fat->cluster_size-offset));
                file->pos+=(int )(fat->cluster_size-offset);
                buf+=fat->cluster_size-offset;
                file->cblock= get_next_block(fat,file->cblock);
            } else{// 不足一个block
                kernel_memcpy(buf,fat->buff+offset,(int )end_pos-file->pos);
                file->pos=(int )end_pos;
            }
        }
    }
    return (int )need_read;
}

int alloc_block(fat_t *fat) {
    // int all=(int )fat->fat_size/ 2;
    // 从第二项目开始分配
    for (int i = 2; i < 1024; ++i) {
        // 寻找空闲项目
        if(get_next_block(fat,i)==FAT_FREE){
            return i;
        }
    }
    return 0;
}

int extend_size(fat_t *fat, int block, int size) {
    // 分批完成了
    if(size<=0){
        return FAT_END;
    }
    // 无效值,分批新表项
    if(block==FAT_END||block==FAT_FREE){
        block=alloc_block(fat);
    }
    int next=get_next_block(fat,block);
    next= extend_size(fat,next,(int )(size-fat->cluster_size));
    set_next_block(fat,block,(int16_t)next);
    return block;
}

int fatfs_write(char *buf, int size, file_t *file){
    fs_t *fs=file->fs;
    fat_t *fat=&fs->fat;
    // 容量不够进行扩展
    if(file->pos+size>file->size){
        file->sblock = extend_size(fat,file->sblock,file->pos+size);
        file->cblock= file->sblock;
        file->size=file->pos+size;
    }

    // 写数据
    while (size){
        // 获取当前位置在 簇内的偏移 与当前簇的剩余空间
        int offset = (int )(file->pos %fat->cluster_size);
        int last = (int )(fat->cluster_size-offset);
        // 注意最开始的簇是2 从2开始 获取簇号计算扇区号
        int sector = (int )(fat->data_start+(file->cblock-2)*fat->cluster_count);
        // 先读取到fat缓冲区中，并废弃缓冲标识
        dev_read(fs->dev_id,sector,fat->buff, (int )fat->cluster_count);
        fat->current_sector=-1;
        // 可以写满
        if(size>=last){
            kernel_memcpy(fat->buff+offset,buf,last);
            buf+=last;
            size-=last;
            file->pos+=last;
            file->cblock= get_next_block(fat,file->cblock);
        } else{// 不能写满
            kernel_memcpy(fat->buff+offset,buf,size);
            size-=size;
            file->pos+=size;
        }
        // 写会数据
        dev_write(fs->dev_id,sector,fat->buff,(int )fat->cluster_count);
    }
    return 0;
}

void fatfs_close(file_t *file){
    fat_t *fat=&file->fs->fat;
    file_item_t *item= read_file_item(fat,file->index);
    // 防止修改申请新空间
    file_item_t temp;
    kernel_memcpy(&temp,item, sizeof(file_item_t));
    // 只修改文件大小与 簇号
    temp.size=file->size;
    temp.clu_lo=file->sblock;
    temp.clu_hi=0;
    write_file_item(fat,&temp,file->index);
}

int fatfs_seek(file_t *file,uint32_t offset,int  dir){
    if(dir!=0){ // 展示只支持从开始的绝对偏移
        return -1;
    }

    // 设置对应位置
    file->pos=(int )offset;
    file->cblock=file->sblock;

    // 每满一个block就移动到下一个block
    fat_t *fat=&file->fs->fat;
    while (offset>=fat->cluster_size){
        offset-=fat->cluster_size;
        file->cblock= get_next_block(fat,file->cblock);
    }
    return 0;
}

int fatfs_stat(file_t *file,struct stat *st){
    return -1;
}

fs_op_t fatfs_op={
        .mount=fatfs_mount,
        .unmount=fatfs_unmount,
        .open=fatfs_open,
        .read=fatfs_read,
        .write=fatfs_write,
        .close=fatfs_close,
        .seek=fatfs_seek,
        .stat=fatfs_stat,
};


// 不再作为系统调用实现
static dir_t dirs[32]; // 暂定 1024个

dir_t *open_dir(char *path){
    // 分配 dir
    dir_t *dir;
    for (int i = 0; i < 1024; ++i) {
        dir=dirs+i;
        if(!dir->use){
            dir->use=1;
            break;
        }
    }
    dir->index=0;
    return dir;
}

dir_item_t *read_dir(dir_t *dir){
    fs_t *fs = get_root_fs();
    fat_t *fat=&fs->fat;
    while (1){
        file_item_t *item=read_file_item(fat,dir->index);
        dir->index++;
        if(item->name[0]==NAME_EMPTY){
            continue;
        }
        if(item->name[0]==NAME_END){
            return 0;
        }

        dir->item.type=file_item_type(item->attr);
        kernel_memset(dir->item.name,0, sizeof(dir->item.name));
        // 8+3 的名称结构 一共11位 前8位为名称 后3位为扩展名
        kernel_memcpy(dir->item.name,item->name,11);
        dir->item.size=item->size;
        dir->item.index=dir->index-1;

        if(dir->item.type==FILE_NORMAL|| dir->item.type==FILE_DIR){
            return &dir->item;
        }
    }
}

void close_dir(dir_t *dir){
    dir->use=0;
}

void sys_ls(char *path){
    dir_t *dir= open_dir(path);
    dir_item_t *item;
    while ((item= read_dir(dir))!=0){
        log_printf("type = %d , name = %s , size = %d\n",item->type,item->name,item->size);
    }
    close_dir(dir);
}

void clear_block(fat_t *fat, int block) {
    while (block){
        // 先获取其指向的下一个值
        int next=get_next_block(fat,block);
        // 再清除其指向的下一个值
        set_next_block(fat,block,0);
        // 进行循环
        block=next;
    }
}

void sys_unlink(char *path){
    fs_t *fs=get_root_fs();
    fat_t *fat=&fs->fat;
    // 会提前因为 NAME_END 而跳出
    for (int i = 0; i < 100; ++i) {
        file_item_t *item=read_file_item(fat,i);
        if(item->name[0]==NAME_EMPTY){
            continue;
        }
        if(item->name[0]==NAME_END){ // 没有找到 不删除 退出
            break;
        }
        if(!file_name_match(path,(char *)item->name)){
            continue;
        }
        // 找到了先清除对应的存储链表，再标记表项为空
        int block = (item->clu_hi<<16)|item->clu_lo;
        clear_block(fat,block); // 只是标记删除
        // 这里不能使用item，因为其位于fat的缓存中，里面读取内容会重新覆盖 item的值，其值带不进入
        file_item_t temp;
        temp.name[0]=NAME_EMPTY;
        write_file_item(fat,&temp,i);
        return;
    }
}