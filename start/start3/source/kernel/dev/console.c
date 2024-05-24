#include "dev/console.h"
#include "comm/cpu_instr.h"

// 与tty对应
#define CONSOLE_NUM 8

static console_t consoles[CONSOLE_NUM];

int read_cursor(){
    int pos;

    outb(0x3D4,0xF);
    pos= inb(0x3D5); // 先读取低8位
    outb(0x3D4,0xE);
    pos|= inb(0x3D5)<<8;// 再读取高8位
    return pos;
}

void write_cursor(int idx,int pos){
    console_t *console=consoles+idx;
    pos+=(int )console->disp_base-CONSOLE_DISP_ADDR; // 鼠标位置需要加相对于基地址的偏移
    outb(0x3D4,0xF);
    outb(0x3D5,(uint8_t)pos);// 先写入低8位
    outb(0x3D4,0xE);
    outb(0x3D5,(uint8_t)(pos>>8));// 再写入高8位
}

void clear_display(console_t *console) {
    int size = console->disp_row*console->disp_col;
    for (int i = 0; i < size; ++i) {
        disp_char_t *ch=console->disp_base+i;
        ch->c=' ';
        ch->foreground=console->foreground;
        ch->background=console->background;
    }
}

void console_init(){
    for (int i = 0; i < CONSOLE_NUM; ++i) {
        console_t *console=consoles+i;
        console->disp_base=(disp_char_t *)CONSOLE_DISP_ADDR+i*CONSOLE_COL*CONSOLE_ROW;
        console->disp_col=CONSOLE_COL;
        console->disp_row=CONSOLE_ROW;
        console->foreground=COLOR_White;
        console->background=COLOR_Black;
        clear_display(console);
    }
}

void write_char(console_t *console, char *ch) {
    disp_char_t *temp=console->disp_base+console->cursor;
    temp->c=*ch;
    temp->background=console->background;
    temp->foreground=console->foreground;
    console->cursor++;
}

void scroll_up(console_t *console, int lines) {
    disp_char_t *dest=console->disp_base;
    disp_char_t *src=console->disp_base+console->disp_col*lines;
    // 上移
    int size = (console->disp_row-lines)*console->disp_col;
    for (int i = 0; i < size; ++i) {
        dest->v=src->v; // 属性字符全部拷贝
        dest++;
        src++;
    }
    // 清空剩余的
    size=lines*console->disp_col;
    for (int i = 0; i < size; ++i) {
        dest->c=' ';
        dest++;
    }
    // 指针上移动
    console->cursor=(console->cursor/console->disp_col-lines)*console->disp_col;
}

void enter(console_t *console) {
    console->cursor=(console->cursor/console->disp_col+1)*console->disp_col;
    if(console->cursor>=console->disp_col*console->disp_row){
        scroll_up(console,1);
    }
}

void console_write(int console, char *data, int size){
    console_t *temp=consoles+console;
    for (int i = 0; i < size; ++i) {
        char *ch = data+i;
        if(*ch=='\n'){
            enter(temp);
        } else if(*ch == 0x7F){
            if(temp->cursor>0){
                temp->cursor--;
                disp_char_t *c=temp->disp_base+temp->cursor;
                c->c=' ';
            }
        } else if(*ch=='\b'){
            if(temp->cursor>0){
                temp->cursor--;
            }
        } else if(*ch < 0x10){ // 控制前景色
            temp->foreground=*ch;
        } else if(*ch < 0x20){ // 控制背景色
            temp->background=(char)(*ch&0x0F);
        } else{
            write_char(temp,data+i);
        }
    }
    write_cursor(console,temp->cursor);
}

void console_close(int console){

}

void console_select(int idx){
    console_t *console = consoles+idx;
    uint16_t pos = (uint32_t)console->disp_base-CONSOLE_DISP_ADDR; // 计算显存基地址偏移
    // 写入基地址偏移的高8位与低8位
    outb(0x3D4,0xC);
    outb(0x3D5,(uint8_t)(pos>>8));
    outb(0x3D4,0xD);
    outb(0x3D5,(uint8_t)pos);
    write_cursor(idx,console->cursor);
}