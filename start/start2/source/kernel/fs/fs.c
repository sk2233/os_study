#include "fs/fs.h"
#include "comm/types.h"
#include "comm/cpu_instr.h"
#include "comm/boot_info.h"
#include "tools/klib.h"

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

static uint8_t buff[100*1024];
static uint8_t *buff_pos;

int sys_open(char *name,int mode){
    // 先读取到内存，初始化指针到内存初始处
    read_disk(5000,80,buff);
    buff_pos=buff;
    return 0;
}

int sys_read(int file,char *tar,int len){
    kernel_memcpy(tar,buff_pos,len);
    buff_pos+=len;
    return len;
}

int sys_write(int file,char *src,int len){
    return -1;
}

int sys_seek(int file,int offset,int dir){
    buff_pos=buff+offset; // 认为传入的是绝对位置
    return 0;
}

int sys_close(int file){
    return 0;
}