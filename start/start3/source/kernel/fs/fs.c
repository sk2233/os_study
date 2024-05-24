/**
 * 文件系统相关接口的实现
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include <stdbool.h>
#include "core/task.h"
#include "comm/cpu_instr.h"
#include "tools/klib.h"
#include "fs/fs.h"
#include "comm/boot_info.h"
#include "dev/dev.h"
#include "tools/list.h"
#include "tools/log.h"

#define TEMP_FILE_ID		100
#define TEMP_ADDR        	(8*1024*1024)      // 在0x800000处缓存原始
#define FS_TAB_SIZE 16

static list_t free_list;
static list_t mount_list;
static fs_t fs_tab[FS_TAB_SIZE];
static fs_t *root_fs;

static uint8_t * temp_pos;       // 当前位置

/**
* 使用LBA48位模式读取磁盘
*/
static void read_disk(int sector, int sector_count, uint8_t * buf) {
    outb(0x1F6, (uint8_t) (0xE0));

	outb(0x1F2, (uint8_t) (sector_count >> 8));
    outb(0x1F3, (uint8_t) (sector >> 24));		// LBA参数的24~31位
    outb(0x1F4, (uint8_t) (0));					// LBA参数的32~39位
    outb(0x1F5, (uint8_t) (0));					// LBA参数的40~47位

    outb(0x1F2, (uint8_t) (sector_count));
	outb(0x1F3, (uint8_t) (sector));			// LBA参数的0~7位
	outb(0x1F4, (uint8_t) (sector >> 8));		// LBA参数的8~15位
	outb(0x1F5, (uint8_t) (sector >> 16));		// LBA参数的16~23位

	outb(0x1F7, (uint8_t) 0x24);

	// 读取数据
	uint16_t *data_buf = (uint16_t*) buf;
	while (sector_count-- > 0) {
		// 每次扇区读之前都要检查，等待数据就绪
		while ((inb(0x1F7) & 0x88) != 0x8) {}

		// 读取并将数据写入到缓存中
		for (int i = 0; i < SECTOR_SIZE / 2; i++) {
			*data_buf++ = inw(0x1F0);
		}
	}
}

bool path_begin_with(char *path, char* sub) {
    while (*path&&*sub){
        if(*path!=*sub){
            return false;
        }
        path++;
        sub++;
    }
    return true;
}

char *next_path(char *name) {
    if(*name!='/'){// 不以/开头无需移除
        return name;
    }
    name++;// 必须先移除第一个 /
    while (*name&&*name!='/'){
        name++;
    }
    name++;// 移除 最后一个 /
    return name;
}

/**
 * 打开文件
 */
int sys_open(char *name, int flags, ...) {
    // 临时使用
    if(kernel_strncmp(name,"/shell.elf",10)){
        // 暂时直接从扇区1000上读取, 读取大概40KB，足够了
//        read_disk(5000, 80, (uint8_t *)TEMP_ADDR);
        temp_pos = (uint8_t *)TEMP_ADDR;
        int dev_id=dev_open(DEV_MAJOR_DISK,0xa0,0);
        // 对于硬盘的读取单位是扇区  addr 是扇区   size 也是扇区
        dev_read(dev_id,5000,(char *)TEMP_ADDR,80);
        return TEMP_FILE_ID;
    }

    file_t *file= file_alloc();
    int fd=task_alloc_fd(file);
    fs_t *fs =root_fs; // 默认使用root_fs是一个fat16文件系统
    list_node_t *node= list_first(&mount_list);
    while (node){
        fs_t *temp = list_node_parent(node,fs_t,node);
        // 根据前缀匹配文件系统
        if(path_begin_with(name,temp->mount_point)){
            fs=temp;
            break;
        }
        node= list_node_next(node);
    }

    name=next_path(name);

    file->mode=flags;
    file->fs=fs;
    file->ref=1;
    kernel_strncpy(file->name,name,FILE_NAME_SIZE);
    fs->op->open(fs,name,file);
    return fd;
}

/**
 * 读取文件api
 */
int sys_read(int fd, char *ptr, int len) {
    // 临时逻辑
    if (fd == TEMP_FILE_ID) {
        kernel_memcpy(ptr, temp_pos, len);
        temp_pos += len;
        return len;
    }

    file_t *file=task_file(fd);
    fs_t *fs=file->fs;
    return fs->op->read(ptr,len,file);// 最终定位到tty设备读取
}

/**
 * 写文件
 */
int sys_write(int fd, char *ptr, int len) {
    file_t *file= task_file(fd);
    fs_t *fs=file->fs;
    return fs->op->write(ptr,len,file);
//    if(file==1){// 标准输出流
//        console_write(0,ptr,len);
//    }
//    ptr[len]='\0';
//    log_printf(ptr);
//    return -1;
}

/**
 * 文件访问位置定位
 */
int sys_lseek(int fd, int ptr, int dir) {
    if (fd == TEMP_FILE_ID) {
        temp_pos = (uint8_t *)(ptr + TEMP_ADDR);
        return 0;
    }
    file_t *file= task_file(fd);
    fs_t *fs=file->fs;
    return fs->op->seek(file,ptr,dir);
}

/**
 * 关闭文件
 */
int sys_close(int fd) {
    if (fd == TEMP_FILE_ID) {
        return 0;
    }
    file_t *file= task_file(fd);
    file->ref--;
    if(file->ref<=0){
        fs_t *fs=file->fs;
        fs->op->close(file);
    }
    task_remove_fd(fd);
    return 0;
}

int sys_isatty(int fd){
    file_t *file= task_file(fd);
    return file->type==FILE_TTY;
}

int sys_fstat(int fd,struct stat *st){
    file_t *file= task_file(fd);
    fs_t *fs=file->fs;
    kernel_memset(st,0, sizeof(struct stat));
    return fs->op->stat(file,st);
}

void mount_list_init() {
    list_init(&free_list);
    for (int i = 0; i < FS_TAB_SIZE; ++i) {
        list_insert_last(&free_list,&fs_tab[i].node);
    }
    list_init(&mount_list);
}

extern fs_op_t fatfs_op;
extern fs_op_t devfs_op;

fs_op_t *get_fs_op(int type) {
    switch (type) {
        case FS_DEV:
            return &devfs_op;
        case FS_FAT16:
            return &fatfs_op;
        default:
            return 0;
    }
}

fs_t *mount(int fs_type, char *mount_point, int major, int minor) {
    // 申请节点
    list_node_t *free_node=list_remove_first(&free_list);
    // 设置属性
    fs_t *fs= list_node_parent(free_node,fs_t,node);
    kernel_memset(fs,0, sizeof(fs_t));
    fs->op=get_fs_op(fs_type);
    kernel_memcpy(fs->mount_point,mount_point,32);
    fs->op->mount(fs,major,minor);
    // 移动节点
    list_insert_last(&mount_list,&fs->node);
    return fs;
}

void fs_init(){
    mount_list_init();
    fs_t *fs=mount(FS_DEV,"/dev",0,0);
    // DISK 文件系统 挂载 第二块磁盘的 第2个扇区 b1
    root_fs= mount(FS_FAT16, "/home", DEV_MAJOR_DISK, 0xb1);
}

fs_t *get_root_fs(){
    return root_fs;
}
