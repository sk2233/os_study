#include <stdbool.h>
#include "dev/disk.h"
#include "tools/log.h"
#include "tools/klib.h"
#include "comm/cpu_instr.h"
#include "comm/boot_info.h"
#include "dev/dev.h"
#include "cpu/irq.h"

static disk_t disks[DISK_COUNT];

void print_disk(disk_t *disk) {
    log_printf("disk name = %s , base = %x , size = %d m\n",disk->name,disk->port_base,
               disk->sector_count*disk->sector_size/1024/1024);
    for (int i = 0; i < DISK_PART_SIZE; ++i) {
        part_t *part=disk->parts+i;
        if(part->type==PART_TYPE_INVALID){
            continue;
        }
        log_printf("part name = %s , type = %x , start = %d , total = %d\n",part->name,part->type,part->start_sector,part->total_sector);
    }
}

// 与 read_disk 前面一致
static void disk_send_cmd(disk_t *disk,uint32_t start_sector,uint32_t sector_count,uint8_t cmd){
    outb(DISK_DRIVE(disk), (uint8_t) (0xE0|disk->drive)); // 0xE0 保证高3位为0 开启LBA模式

    outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count >> 8));
    outb(DISK_LBA_LO(disk), (uint8_t) (start_sector >> 24));		// LBA参数的24~31位
    outb(DISK_LBA_MI(disk), (uint8_t) (0));					// LBA参数的32~39位
    outb(DISK_LBA_HI(disk), (uint8_t) (0));					// LBA参数的40~47位

    outb(DISK_SECTOR_COUNT(disk), (uint8_t) (sector_count));
    outb(DISK_LBA_LO(disk), (uint8_t) (start_sector));			// LBA参数的0~7位
    outb(DISK_LBA_MI(disk), (uint8_t) (start_sector >> 8));		// LBA参数的8~15位
    outb(DISK_LBA_HI(disk), (uint8_t) (start_sector >> 16));		// LBA参数的16~23位

    outb(DISK_STAT_CMD(disk), cmd);
}

static void disk_read_data(disk_t *disk,void *buf,int size){
    uint16_t *b = (uint16_t*)buf;
    for (int i = 0; i < size / 2; ++i) {
        *b++= inw(DISK_DATA(disk));
    }
}

static void disk_write_data(disk_t *disk,void *buf,int size){
    uint16_t *b = (uint16_t*)buf;
    for (int i = 0; i < size / 2; ++i) {
        outw(DISK_DATA(disk),*b++);
    }
}

static bool disk_wait_data(disk_t *disk){
    // 忙的话就等待
    while (inb(DISK_STAT_CMD(disk))&DISK_STAT_BUSY){

    }
    return true;
}

void detect_part_info(disk_t *disk) {
    // 读取mbr信息
    mbr_t mbr;
    disk_send_cmd(disk,0,1,DISK_CMD_READ);
    disk_wait_data(disk);
    disk_read_data(disk,&mbr, sizeof(mbr_t));

    // 提取分区信息
    part_item_t *item = mbr.part_items;
    part_t *part=disk->parts+1; // 第一块已经使用过了
    for (int i = 0; i < DISK_PART_SIZE; ++i) {
        part->type=item->system_id;
        if(part->type==PART_TYPE_INVALID){// 不可用舍弃
            part->disk=0;
            part->start_sector=0;
            part->total_sector=0;
        } else{// 可用设置对弈信息
            part->disk=disk;
            part->start_sector=item->relative_sector;
            part->total_sector=item->total_sector;
            kernel_sprintf(part->name,"%s%d",disk->name,i+1);
        }
        part++;
        item++;
    }
}

void identify_disk(disk_t *disk) {
    // 准备数据  后续数据都是对该磁盘操作
    disk_send_cmd(disk,0,0,DISK_CMD_IDENT);
    // 等待准备完毕
    disk_wait_data(disk);
    // 读取磁盘描述结构
    uint16_t buf[256];
    disk_read_data(disk,buf, sizeof(buf));
    // 这里只要磁盘的扇区数 实际使用 64位存储，但是这里磁盘较小 32位只取低位也够用
    disk->sector_count=*(int *)(buf+100);
    disk->sector_size=SECTOR_SIZE;

    // 设定5个分区 第一个是虚拟的 包含全部区域
    part_t *part = disk->parts;
    part->disk=disk;
    kernel_sprintf(part->name,"%s%d",disk->name,0); // sda0  sdb0
    part->start_sector=0;
    part->total_sector=disk->sector_count;
    part->type=PART_TYPE_INVALID;

    detect_part_info(disk);
}

int disk_open(device_t *dev){
    // 使用次设备号分割出磁盘号与扇区号   例如  0xa1  磁盘号 a  扇区号 1
    int disk_idx = (dev->minor>>4)-0xa;
    int part_idx = dev->minor&0xF;
    disk_t *disk=disks+disk_idx;
    if(disk->sector_size==0){
        log_printf("disk not exist sd%x",disk_idx+'a');
    }
    part_t *part=disk->parts+part_idx;
    if(part->type==PART_TYPE_INVALID){
        log_printf("part is invalid sd%x%d",disk_idx+'a',part_idx);
    }
    // 保存对应的分区方便后面操作
    dev->data=part;
    return 0;
}

// 这里 addr size 都是以扇区为单位
int disk_read(device_t *dev,int addr,char *buf,int size){
    part_t *part=dev->data;
    disk_t *disk=part->disk;

    disk_send_cmd(disk,part->start_sector+addr,size,DISK_CMD_READ);
    for (int i = 0; i < size; ++i) {
        disk_wait_data(disk);
        // 一次读一个扇区
        disk_read_data(disk,buf,disk->sector_size);
        buf+=disk->sector_size;
    }
    return size;// 返回的也是扇区数注意
}

int disk_write(device_t *dev,int addr,char *buf,int size){
    part_t *part=dev->data;
    disk_t *disk=part->disk;

    disk_send_cmd(disk,part->start_sector+addr,size,DISK_CMD_WRITE);
    for (int i = 0; i < size; ++i) {
        // 一次写入一个扇区，先写入，再等待
        disk_write_data(disk,buf,disk->sector_size);
        buf+=disk->sector_size;
        disk_wait_data(disk);
    }
    return size;// 返回的也是扇区数注意
}

int disk_ctrl(device_t *dev,int cmd,int arg0,int arg1){

}

int disk_close(device_t *dev){

}

static dev_desc_t dev_disk_desc={
        .name="DISK_DEV",
        .major=DEV_MAJOR_DISK,
        .open=disk_open,
        .read=disk_read,
        .write=disk_write,
        .ctrl=disk_ctrl,
        .close=disk_close
};

void do_handler_disk (exception_frame_t *frame) {
    // 磁盘读取，写入完成也会有自动的中断，可以打开对应的中断利用中断挂起数据读取线程等待中断再唤醒
    pic_send_eoi(IRQ1_KBD);// 标记处理完毕
}

void disk_init(){
    log_printf("check disk...\n");
    kernel_memset(disks,0, sizeof(disks));
    for (int i = 0; i < DISK_COUNT; ++i) {
        disk_t *disk = disks+i;
        // 名字就是 sda sdb ...
        kernel_sprintf(disk->name,"sd%c",i+'a');
        disk->drive=(i==0)?DISK_DRIVE_MASTER:DISK_DRIVE_SLAVE;
        disk->port_base=0x1F0;// 基地址主bus都是这个
        identify_disk(disk);
        print_disk(disk);
    }
    add_dev_desc(&dev_disk_desc);
    // 建立硬盘处理需要的中断，可以帮助读取数据的进程防止忙等
    irq_install(IRQ14_DISK, (irq_handler_t)exception_handler_disk);
    irq_enable(IRQ14_DISK);
}
