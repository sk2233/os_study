ENTRY(_start)
SECTIONS
{
	. = 0x81000000;
	.text : {
		*(*.text)
	}

	.rodata : {
		*(*.rodata)
	}

	.data : {
		*(*.data)
	}

	.bss : {
        PROVIDE(bss_start_ = .);
		*(*.bss)
		PROVIDE(bss_end_ = .);
	}
}
