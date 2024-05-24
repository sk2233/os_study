# 功能：工程目标创建的makefile文件
# 
# 创建时间：2022年8月31日
# 作者：李述铜
# 联系邮箱: 527676163@qq.com
# 相关信息：此工程为《从0写x86 Linux操作系统》的前置课程，用于帮助预先建立对32位x86体系结构的理解
# 课程请见：https://study.163.com/course/introduction.htm?courseId=1212765805&_trace_c_p_k2_=0bdf1e7edda543a8b9a0ad73b5100990

# 工具链前缀，如果是windows和mac，使用x86_64-elf-
# 如果是linux，使用x86_64-linux-gnu-
# ifeq ($(LANG),)
# 	TOOL_PREFIX = x86_64-linux-gnu-
# else
# 	TOOL_PREFIX = x86_64-elf-
# endif

# GCC编译参数
CFLAGS = -g -c -O0 -m32 -fno-pie -fno-stack-protector -nostdlib -nostdinc

# 目标创建:涉及编译(gcc)、链接(ld)、二进制转换(objcopy)、反汇编、写磁盘映像(dd)
gen: source/os.c source/os.h source/start.S
	x86_64-elf-gcc $(CFLAGS) source/start.S 
	x86_64-elf-gcc $(CFLAGS) source/os.c	
	x86_64-elf-ld -m elf_i386 -Ttext=0x7c00 start.o os.o -o os.elf
	x86_64-elf-objcopy -O binary os.elf os.bin  
	dd if=os.bin of=disk.img conv=notrunc
# dd数据拷贝指令   if = input file , of = output file , conv = 可选参数 , notrunc不对目标文件截断

# x86_64-elf-objdump -x -d -S  os.elf > os_dis.txt	
# x86_64-elf-readelf -a  os.elf > os_elf.txt	

# 清理
clr:
	rm -f *.elf *.o *.bin

run:
	qemu-system-i386 -m 128M -s -S  -drive file=disk.img,index=0,media=disk,format=raw