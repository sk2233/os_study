#ifndef TTY_H
#define TTY_H

#define TTY_NUM 8
#define TTY_BUF_SIZE 512

#include "ipc/sem.h"

typedef struct tty_fifo{
    char buf[TTY_BUF_SIZE];
    int read,write; // 读写指针
    int count;
}tty_fifo_t;

int tty_fifo_put(tty_fifo_t *fifo,char ch);
char tty_fifo_get(tty_fifo_t *fifo);

typedef struct tty{
    tty_fifo_t ofifo;
    tty_fifo_t ififo;
    sem_t isem;
    int console_idx;
}tty_t;

void tty_init();
void tty_in(char ch);
void tty_select(int idx);

#endif