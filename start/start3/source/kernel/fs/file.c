#include "fs/file.h"
#include "tools/klib.h"

static file_t file_tab[FILE_NUM];

void file_init(){
}

file_t *file_alloc(){
    for (int i = 0; i < FILE_NUM; ++i) {
        file_t *file=file_tab+i;
        if(file->ref<=0){ // 没有引用就是被释放的
            kernel_memset(file,0, sizeof(file_t));
            return file;
        }
    }
    return 0;
}

void file_free(file_t *file){
    file->ref--;// 减少文件引用数即可
}
