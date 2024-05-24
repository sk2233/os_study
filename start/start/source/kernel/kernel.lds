/* 参考文档： https://ftp.gnu.org/old-gnu/Manuals/ld-2.9.1/html_chapter/ld_3.html */
SECTIONS
{
	PROVIDE(kernel_base = 0x0);

	. = 0x00010000; /* 指定虚拟地址，没有指定物理地址也是这个 */

	PROVIDE(s_text = .);
	.text : {
	/* first_task有自己单独的规则这里需要排除 */
		*(EXCLUDE_FILE(*first_task*) .text)
	}
	.rodata : {
		*(EXCLUDE_FILE(*first_task*) .rodata)
	}
	PROVIDE(e_text = .);

	. = ALIGN(4096); /* 因为对齐的原因会与上面断开 */
	PROVIDE(s_data = .);
	.data : {
		*(EXCLUDE_FILE(*first_task*) .data)
	}
	.bss : {
		*(EXCLUDE_FILE(*first_task*) .bss)
	}
	e_data = .;

    PROVIDE(s_first_task = .);
	. = 0x80000000; /*  从2g虚拟地址的位置开始 */
    .first_task : AT(e_data){ /* 物理地址还是需要连续 */
        *first_task_entry*(.text .rodata .bss .data)
        *first_task*(.text .rodata .bss .data)
    }
    PROVIDE(e_first_task = LOADADDR(.first_task)+SIZEOF(.first_task));

    /* 因为虚拟地址被改了，这个也会向下顺移，但是这里需要的是实际地址，就计算一下位置 */
	PROVIDE(mem_free_start = e_first_task);
}
