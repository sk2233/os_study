#ifndef LIB_SYSCALL
#define LIB_SYSCALL
#include "comm/types.h"
#include "os_cfg.h"

#define SYSCALL_SLEEP 1
#define SYSCALL_GETPID 2
#define SYSCALL_FORK 3

typedef struct syscall_args{
    int  func_id;
    int arg0;
    int arg1;
    int arg2;
    int arg3;
}syscall_args_t;

static inline int syscall(syscall_args_t *args){
    uint32_t addr[] ={0,SELECTOR_SYSCALL};
    int ret;
    // 先压入参数再跳转到目标位置
    __asm__ __volatile__(
            "push %[arg3]\n"
            "push %[arg2]\n"
            "push %[arg1]\n"
            "push %[arg0]\n"
            "push %[func_id]\n"
            "lcalll *(%[a])":
            "=a"(ret):   // 取 exa 的值，函数的返回值实际就存储在 eax 中
    [arg3]"r"(args->arg3),
    [arg2]"r"(args->arg2),
    [arg1]"r"(args->arg1),
    [arg0]"r"(args->arg0),
    [func_id]"r"(args->func_id),
    [a]"r"(addr)
    );
    return ret;
}

static inline void lib_sleep(int ms){
    if(ms<=0){
        return;
    }
    syscall_args_t args; // args本来就是脏的
    args.func_id=SYSCALL_SLEEP;
    args.arg0=ms;
    syscall(&args);
}

static inline int lib_getpid(){
    syscall_args_t args; // args本来就是脏的
    args.func_id=SYSCALL_GETPID;
    return syscall(&args);
}

static inline int lib_fork(){
    syscall_args_t args; // args本来就是脏的
    args.func_id=SYSCALL_FORK;
    return syscall(&args);
}

#endif