#include "dev/dev.h"

#define DEV_DESC_COUNT 16
#define DEVICE_COUNT 128

static dev_desc_t *dev_descs[DEV_DESC_COUNT];
static device_t devices[DEVICE_COUNT];

void add_dev_desc(dev_desc_t *dev_desc){
     dev_descs[dev_desc->major]=dev_desc;
}

int dev_open(int major,int minor,void *data){// 打开设备并返回设备id
    if(dev_descs[major]->major==0){// 设备类型不存在
        return -1;
    }

    // 判断是否已经有存在的了
    for (int i = 0; i < DEVICE_COUNT; ++i) {
        device_t *device=devices+i;
        if(device->open_count>0&&device->desc->major==major&&device->minor==minor){// 重复打开
            device->open_count++;
            return i;
        }
    }

    // 找空位置放置新设置
    int dev_id =-1;
    for (int i = 0; i < DEVICE_COUNT; ++i) {
        device_t *temp=devices+i;
        if(temp->open_count==0){
            dev_id=i;
            break;
        }
    }
    if(dev_id==-1){
        return -1;
    }

    // 初始化设备
    device_t *device=devices+dev_id;
    device->open_count++;
    device->minor=minor;
    device->desc=dev_descs[major];
    device->data=data;
    device->desc->open(device);
    return dev_id;
}

int dev_read(int dev_id,int addr,char *buf,int size){
    device_t *device=devices+dev_id;
    return device->desc->read(device,addr,buf,size);
}

int dev_write(int dev_id,int addr,char *buf,int size){
    device_t *device=devices+dev_id;
    return device->desc->write(device,addr,buf,size);
}

int dev_ctrl(int dev_id,int cmd,int arg0,int arg1){
    device_t *device=devices+dev_id;
    return device->desc->ctrl(device,cmd,arg0,arg1);
}

void dev_close(int dev_id){
    device_t *device=devices+dev_id;
    device->open_count--;
    if(device->open_count<=0){
        device->desc->close(device);
        device->open_count=0;
    }
}