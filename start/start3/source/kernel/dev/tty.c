#include "dev/tty.h"
#include "dev/dev.h"
#include "dev/console.h"
#include "ipc/sem.h"

// 与屏幕输出一一对应
static tty_t ttys[TTY_NUM];
static int tty_idx=0;

int tty_fifo_put(tty_fifo_t *fifo,char ch){
    if(fifo->count>=TTY_BUF_SIZE){
        return -1;
    }
    fifo->buf[fifo->write]=ch;
    fifo->write=(fifo->write+1)%TTY_BUF_SIZE;
    fifo->count++;
    return 0;
}

char tty_fifo_get(tty_fifo_t *fifo){
    if(fifo->count<=0){
        return 0;
    }
    char res = fifo->buf[fifo->read];
    fifo->read=(fifo->read+1)%TTY_BUF_SIZE;
    fifo->count--;
    return res;
}

int tty_open(device_t *dev){// 打开无需初始化 其minor就是 tty与屏幕输出的索引
    tty_t *tty=ttys+dev->minor;
    tty->console_idx= dev->minor; // 随便记录一下
    sem_init(&tty->isem,0); // 用来读取字符的默认初始化为0
    return 0;
}

int tty_read(device_t *dev,int addr,char *buf,int size){
    tty_t *tty=ttys+dev->minor;
    int need = size;
    while (need>0){
        sem_wait(&tty->isem);// 有数据就进行读取
        char ch=tty_fifo_get(&tty->ififo);
        *buf++= ch;
        need--;
        console_write(tty->console_idx,&ch,1); // 直接显示出来
        if(ch=='\n'){ // 回车提前结束
            return size-need;
        }
    }
    return size;
}

void tty_in(char ch){
    tty_t *tty=ttys+tty_idx;
    tty_fifo_put(&tty->ififo,ch); // 写入缓存并通知进程可以使用了
    sem_notify(&tty->isem);
}

void tty_select(int idx){
    if(idx==tty_idx){// 只有不同才触发切换
        return;
    }
    tty_idx=idx;
    console_select(idx);
}

int tty_write(device_t *dev,int addr,char *buf,int size){
    console_write(dev->minor,buf,size);
    return size;
}

int tty_ctrl(device_t *dev,int cmd,int arg0,int arg1){
    return 0;
}

int tty_close(device_t *dev){
    return 0;
}

static dev_desc_t dev_tty_desc={
        .name="TTY_DEV",
        .major=DEV_MAJOR_TTY,
        .open=tty_open,
        .read=tty_read,
        .write=tty_write,
        .ctrl=tty_ctrl,
        .close=tty_close
};

void tty_init(){
    add_dev_desc(&dev_tty_desc);
}