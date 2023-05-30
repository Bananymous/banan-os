ENTRY (_start)

KERNEL_OFFSET = 0xFFFFFFFF80000000;

SECTIONS
{
	. = 0x00100000 + KERNEL_OFFSET;

	g_kernel_start = .;
	.text ALIGN(4K) : AT(ADDR(.text) - KERNEL_OFFSET)
	{
		*(.multiboot)
		*(.text)
	}
	.rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_OFFSET)
	{
		*(.rodata.*)
	}
	.data ALIGN(4K) : AT(ADDR(.data) - KERNEL_OFFSET)
	{
		*(.data)
	}
	.bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_OFFSET)
	{
		*(COMMON)
		*(.bss)
	}
	g_kernel_end = .;
}