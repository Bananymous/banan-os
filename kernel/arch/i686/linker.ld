ENTRY (_start)

KERNEL_OFFSET = 0xC0000000;

SECTIONS
{
	. = 0x00100000 + KERNEL_OFFSET;

	g_kernel_start = .;
	.text ALIGN(4K) : AT(ADDR(.text) - KERNEL_OFFSET)
	{
		g_kernel_execute_start = .;
		*(.multiboot)
		*(.bananboot)
		*(.text.*)
	}
	.userspace ALIGN(4K) : AT(ADDR(.userspace) - KERNEL_OFFSET)
	{
		g_userspace_start = .;
		*(.userspace)
		g_userspace_end = .;
		g_kernel_execute_end = .;
	}
	.ap_init ALIGN(4K) : AT(ADDR(.ap_init) - KERNEL_OFFSET)
	{
		g_ap_init_addr = .;
		*(.ap_init)
	}
	.rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_OFFSET)
	{
		*(.rodata.*)
	}
	.init_array ALIGN(4K) : AT(ADDR(.init_array) - KERNEL_OFFSET)
	{
		g_init_array_start = .;
		*(.init_array)
		g_init_array_end = .;
	}
	.data ALIGN(4K) : AT(ADDR(.data) - KERNEL_OFFSET)
	{
		g_kernel_writable_start = .;
		*(.data)
	}
	.bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_OFFSET)
	{
		*(COMMON)
		*(.bss)
		g_kernel_writable_end = .;
	}
	g_kernel_end = .;
}
