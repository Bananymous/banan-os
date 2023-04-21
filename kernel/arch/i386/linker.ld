ENTRY (_start)

SECTIONS
{
	. = 0x00100000;

	g_kernel_start = .;
	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata.*)
	}
	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}
	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}

	g_kernel_end = .;

	. = 0x00A00000;

	g_userspace_start = .;
	.userspace BLOCK(4K) : ALIGN(4K)
	{
		*(.userspace)
	}
	g_userspace_end = .;
}