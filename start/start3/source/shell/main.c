/**
 * 简单的命令行解释器
 *
 * 作者：李述铜
 * 联系邮箱: 527676163@qq.com
 */
#include "lib_syscall.h"
#include "stdio.h"
#include "main.h"
#include "string.h"
#include "stdlib.h"

static cli_t cli;
static cli_cmd_t cmds[]={
        {
            .name="help",
            .tip="show all cmd",
            .handler=do_help,
        },
        {
                .name="echo",
                .tip="show input msg",
                .handler=do_echo,
        },
        {
                .name="exit",
                .tip="exit task",
                .handler=do_exit,
        },{
                .name="ls",
                .tip="list file",
                .handler=do_ls,
        },{
                .name="cat",
                .tip="show file",
                .handler=do_less,
        },{
                .name="cp",
                .tip="cp src desc",
                .handler=do_cp,
        },{
                .name="rm",
                .tip="rm src",
                .handler=do_rm,
        }
};
static int cmd_size = sizeof(cmds)/ sizeof(cli_cmd_t);

static void cli_init(){
    cli.prefix="sh >>";
}

void show_prefix() {
    printf("%s",cli.prefix);
    fflush(stdout);//刷新使立即输出
}

cli_cmd_t *find_cmd(char *name) {
    for (int i = 0; i < cmd_size; ++i) {
        cli_cmd_t *cmd = cmds+i;
        if(strcmp(cmd->name,name)==0){
            return cmd;
        }
    }
    return 0;
}

int run_exec(char *path, int argc, char **argv) {
    int pid =fork();
    if(pid==0){// 子进程
        for (int i = 0; i < argc; ++i) {
            printf("arg %d = %s\n",i,argv[i]);
        }
        exit(-1);
    } else{// 父进程
        int code;
        int pid= wait(&code); // 等待子进程结束获取其id与状态，方便回收资源
        printf("wait pid %d , code %d \n",pid,code);
    }
    return 1;
}

int main (int argc, char **argv) {
    // 这里 stdin,stdout,stderr都是同一个文件
    int fd=open("/dev/tty0",0); // 打开设备文件   stdin(0)
    int d1= dup(fd); // 复制stdin到stdout(1)
    int d2= dup(fd); // 复制stdin到stderr(2)
//    printf("HELLO\n");// 这个输出必须遇到 \n才会一并输出
//    printf("WORLD\x02\n");
    for (int i = 0; i < argc; i++) {
        printf("arg: %d\n", (int)argv[i]);
    }
    // 创建一个自己的副本
//    fork();
//    yield();
    cli_init();
    for (;;) {
        show_prefix();
        gets(cli.input);
        int argc =0;
        char *argv[10];
        char *token = strtok(cli.input," ");
        while (token){
            argv[argc++]=token;
            token= strtok(NULL," ");
        }

        if(argc<=0){
            continue;
        }
        cli_cmd_t *cmd= find_cmd(argv[0]);
        if(cmd){
            int code =cmd->handler(argc,argv);
            if(code){
                printf("errcode = %d\n",code);
            }
            continue;
        }
        int code = run_exec(argv[0],argc,argv);
        if(code){
            continue;
        }

        printf("\x4unknown cmd or file : %s\xF\n",argv[0]);
//        gets(buff); // 从系统读取字符 最终调用 sys_read
//        puts(buff); // 输出到屏幕
//        printf("pid=%d\n", getpid());
//        msleep(1000);
    }
}

//===================cmd====================

int do_cp(int argc,char **argv){
    if(argc<3){
        printf("argc = %d err",argc);
        return -1;
    }

    FILE *src = fopen(argv[1],"rb");
    // 没有对应的文件会自动创建
    FILE *desc = fopen(argv[2],"wb");
    char *buff = (char *) malloc(256);
    size_t size;
    while ((size= fread(buff,1,256,src))>0){
        fwrite(buff,1,size,desc);
    }
    free(buff);

    fclose(src);
    fclose(desc);
    return 0;
}

int do_less(int argc,char **argv){
    if(argc<2){
        printf("no file to open\n");
        return -1;
    }
    FILE *file= fopen(argv[1],"r");
    char *buf=(char *) malloc(256);
    while (fgets(buf,256,file)!=NULL){
        fputs(buf,stdout);
    }
    free(buf);
    fclose(file);
    return 0;
}

int do_rm(int argc,char **argv){
    if(argc<2){
        printf("no file to remove");
        return -1;
    }
    unlink(argv[1]);
    return 0;
}

int do_ls(int argc,char **argv){
    ls("/home");
    return 0;
}

int do_help(int argc,char **argv){
    for (int i = 0; i < cmd_size; ++i) {
        cli_cmd_t *cmd = cmds+i;
        printf("name:\t%s\n",cmd->name);
        printf("tip:\t%s\n",cmd->tip);
        printf("==========================\n");
    }
    return 0;
}

int do_echo(int argc,char **argv){
    for (int i = 1; i < argc; ++i) {
        printf("%s ",argv[i]);
    }
    printf("\n");
    return 0;
}

int do_exit(int argc,char **argv){
    exit(0);
    return 0;
}