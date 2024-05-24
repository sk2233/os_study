#ifndef MAIN_H
#define MAIN_H

#define CLI_INPUT_SIZE 256

typedef struct cli_cmd{
    char *name;
    char *tip;
    int (*handler)(int argc,char **argv);
}cli_cmd_t;

typedef struct cli{
    char input[CLI_INPUT_SIZE];
    char *prefix;
}cli_t;

//===============cmd=================

int do_help(int argc,char **argv);
int do_echo(int argc,char **argv);
int do_exit(int argc,char **argv);
int do_ls(int argc,char **argv);
int do_less(int argc,char **argv);
int do_cp(int argc,char **argv);
int do_rm(int argc,char **argv);

#endif