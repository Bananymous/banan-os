SECTIONS
{
	. = 0x7C00;
	.stage1 : { *(.stage1) }

	. = ALIGN(512);
	.stage2 : { *(.stage2) }

	. = ALIGN(512);
	.bss : { *(.bss) }
}