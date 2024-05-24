#include "core/syscall.h"
#include "lib/lib_syscall.h"
#include "core/task.h"

void do_handler_syscall(syscall_frame_t *frame){
    // 根据不同函数标识调用不同函数
    switch (frame->func_id) {
        case SYSCALL_SLEEP:
            sys_sleep(frame->arg0);
            break;
        case SYSCALL_GETPID:
            // 即使函数没有声明返回值，依旧可以进行值返回，默认的返回值实际就是 eax
            frame->eax=sys_getpid();
            break;
        case SYSCALL_FORK:
            frame->eax=sys_fork(frame);
            break;
    }
}