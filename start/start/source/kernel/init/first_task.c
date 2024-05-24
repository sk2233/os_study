/**
 * 内核初始化以及测试代码
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "core/task.h"
#include "tools/log.h"
#include "lib/lib_syscall.h"

int first_task_main (void) {
    log_printf("main");

    uint32_t pid= lib_fork();
    if(pid>0){
        log_printf("parent , pid = %d",pid);
    } else{
        log_printf("child , pid = %d",pid);
    }

    for (;;) {
        sys_sleep(1000);
    }
    return 0;
} 