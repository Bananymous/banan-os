ENTRY (_start)

SECTIONS
{
	. = 0x00100000;

	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
		*(.text)
	}
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		g_rodata_start = .;
		*(.rodata.*)
		g_rodata_end = .;
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