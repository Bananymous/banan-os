ENTRY(stage1_main)

SECTIONS
{
	. = 0x7C00;
	.stage1 : { *(.stage1) }

	. = ALIGN(512);
	stage2_start = .;
	.stage2 : { *(.stage2) }
	stage2_end = .;

	. = ALIGN(512);
	.bss : { *(.bss) }
}