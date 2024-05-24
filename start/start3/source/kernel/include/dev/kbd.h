#ifndef KBD_H
#define KBD_H

// 数据读取端口
#define KBD_PORT_DATA 0x60
// 状态读取,命令写入端口
#define KBD_PORT_CMD 0x64
#define KBD_STAT_READY (1<<0)

#define KBD_STATE_SHIFT (1<<0)
#define KBD_STATE_CTRL  (1<<1)
#define KBD_STATE_ALT   (1<<2)

void kbd_init();
void exception_handler_kbd ();

#endif