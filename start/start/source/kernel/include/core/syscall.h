#ifndef SYSCALL
#define SYSCALL
#include "comm/types.h"

#define SYSCALL_PARAM_COUNT 5

void handler_syscall();

typedef struct syscall_frame{
    uint32_t eflags;
    uint32_t gs,fs,es,ds;
    uint32_t edi,esi,ebp,unused,ebx,edx,ecx,eax;
    uint32_t eip,cs;
    uint32_t func_id,arg0,arg1,arg2,arg3;
    uint32_t esp,ss;
}syscall_frame_t;

#endif