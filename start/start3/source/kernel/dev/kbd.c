#include "dev/kbd.h"
#include "cpu/irq.h"
#include "comm/cpu_instr.h"
#include "tools/log.h"
#include "dev/tty.h"

void kbd_init(){
    irq_install(IRQ1_KBD, (irq_handler_t)exception_handler_kbd);
    irq_enable(IRQ1_KBD);
}

static char *normal="#`1234567890-=\x7F#qwertyuiop[]\n#asdfghjkl;'###zxcvbnm#.#### ";
static char *shift=  "#~!@#$%^&*()_+\x7F#QWERTYUIOP{}\n#ASDFGHJKL:'###ZXCVBNM#>#### ";
static uint8_t kbd_state=0;

char get_char(uint8_t code){
    code&=0x7F; // 移除按下松开的影响
    if(kbd_state&KBD_STATE_SHIFT){
        return *(shift + code);
    }
    return *(normal + code);
}

uint8_t is_press(uint8_t code){// 按下松开相差 128 松开都是 128起步的
    return !(code&0x80);
}

uint8_t handle_ctrl(uint8_t code) { // 都是仅处理左边的按钮
    if(kbd_state&KBD_STATE_CTRL){ // 切换屏幕
        uint8_t num=code-2; // 从1开始 映射到 tty0~tty7
        if(num>=0&&num<8){// 确实在切换
            tty_select(num);
            return 1;
        }
    }
    if(code==42){
        kbd_state|=KBD_STATE_SHIFT;
        return 1;
    }
    if(code==170){
        kbd_state&=~KBD_STATE_SHIFT;
        return 1;
    }
    if(code==29){
        kbd_state|=KBD_STATE_CTRL;
        return 1;
    }
    if(code==157){
        kbd_state&=~KBD_STATE_CTRL;
        return 1;
    }
    if(code==56){
        kbd_state|=KBD_STATE_ALT;
        return 1;
    }
    if(code==184){
        kbd_state&=~KBD_STATE_ALT;
        return 1;
    }
    return 0;
}

void do_handler_kbd (exception_frame_t *frame) {
    uint8_t state=inb(KBD_PORT_CMD);// 读取状态
    if(state&KBD_STAT_READY){// 判断是否有数据
        uint8_t code = inb(KBD_PORT_DATA);
        if(!handle_ctrl(code)&&is_press(code)){
            char ch = get_char(code);
            tty_in(ch); // 写入 tty0号设备
//            log_printf("code = %d char = %c",code,ch);
        }
    }
    pic_send_eoi(IRQ1_KBD);// 标记处理完毕
}