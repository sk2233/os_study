#ifndef FS_H
#define FS_H

int sys_open(char *name,int mode);
int sys_read(int file,char *tar,int len);
int sys_write(int file,char *src,int len);
int sys_seek(int file,int offset,int dir);
int sys_close(int file);

#endif