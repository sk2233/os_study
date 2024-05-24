/**
 * 进程启动C部分代码
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "lib_syscall.h"
#include "stdlib.h"

int main (int argc, char ** argv);

extern uint8_t bss_start_[],bss_end_[]; // 对应程序的 lds文件必须包含这两个定义

/**
 * @brief 应用的初始化，C部分
 */
void cstart (int argc, char ** argv) {
    // 对bss数据段进行预先清零操作
    uint8_t *item = bss_start_;
    while (item<bss_end_){
        *item=0;
        item++;
    }
    exit(main(argc, argv));
}