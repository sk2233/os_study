#ifndef DEV_H
#define DEV_H

#define DEV_NAME_SIZE 32

#define DEV_MAJOR_TTY 1
#define DEV_MAJOR_DISK 2

struct dev_desc; // 提前声明类型提前使用
typedef struct dev_desc dev_desc_t; // 起别名，方便使用

typedef struct device{// 具体某一个设备
    dev_desc_t *desc;
    int  mode;
    int minor;
    void *data;
    int open_count;
}device_t;

struct dev_desc{ // 定义某一类设备
    char name[DEV_NAME_SIZE];
    int major;
    int (*open)(device_t *dev); // 这类设备提供的操作方法指针
    int (*read)(device_t *dev,int addr,char *buf,int size);
    int (*write)(device_t *dev,int addr,char *buf,int size);
    int (*ctrl)(device_t *dev,int cmd,int arg0,int arg1);
    int (*close)(device_t *dev);
};

void add_dev_desc(dev_desc_t *dev_desc);

int dev_open(int major,int minor,void *data);
int dev_read(int dev_id,int addr,char *buf,int size);
int dev_write(int dev_id,int addr,char *buf,int size);
int dev_ctrl(int dev_id,int cmd,int arg0,int arg1);
void dev_close(int dev_id);

#endif