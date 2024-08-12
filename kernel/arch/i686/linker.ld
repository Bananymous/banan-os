ENTRY (_start)

KERNEL_OFFSET = 0xC0000000;

SECTIONS
{
	. = 0xF000;
	.ap_init ALIGN(4K) : AT(ADDR(.ap_init))
	{
		g_ap_init_addr = .;
		*(.ap_init)
	}

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
	.rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_OFFSET)
	{
		*(.rodata.*)
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
