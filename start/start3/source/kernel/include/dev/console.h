#ifndef CONSOLE_H
#define CONSOLE_H

#include "comm/types.h"

#define CONSOLE_DISP_ADDR 0xb8000
#define CONSOLE_DISP_END (0xb8000+32*1024)
#define CONSOLE_ROW 25
#define CONSOLE_COL 80

// 各种颜色
#define COLOR_Black            0
#define COLOR_Blue             1
#define COLOR_Green            2
#define COLOR_Cyan             3
#define COLOR_Red              4
#define COLOR_Magenta          5
#define COLOR_Brown            6
#define COLOR_Gray             7
#define COLOR_Dark_Gray        8
#define COLOR_Light_Blue       9
#define COLOR_Light_Green      10
#define COLOR_Light_Cyan       11
#define COLOR_Light_Red        12
#define COLOR_Light_Magenta    13
#define COLOR_Yellow           14
#define COLOR_White            15


typedef union _disp_char_t {
    struct {
        char c;
        char foreground: 4;
        char background: 3;
    };
    uint16_t v;
} disp_char_t;

typedef struct _console_t {
    disp_char_t *disp_base;
    int disp_row, disp_col;
    int cursor;
    char foreground, background;
} console_t;

void console_init();

void console_write(int console, char *data, int size);

void console_close(int console);

void console_select(int idx);

#endif