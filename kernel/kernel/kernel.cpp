#include <kernel/GDT.h>
#include <kernel/kmalloc.h>
#include <kernel/panic.h>
#include <kernel/tty.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

// https://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Boot-information-format
struct multiboot_info_t
{
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t syms[4];
	uint32_t mmap_length;
	uint32_t mmap_addr;
	uint32_t drives_length;
	uint32_t drives_addr;
	uint32_t config_table;
	uint32_t boot_loader_name;
	uint32_t apm_table;
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;
	uint8_t framebuffer[22];
#if 1
	uint8_t  color_info[6];
#endif
} __attribute__((packed));

extern "C"
void kernel_main(multiboot_info_t* mbi, uint32_t magic)
{
	DISABLE_INTERRUPTS();

	terminal_initialize();

	if (magic != 0x2BADB002)
		Kernel::panic("Invalid magic in multiboot");
	if (mbi->flags & 0b00100000)
	{
		printf("mmap_length: %d\n", mbi->mmap_length);
		printf("mmap_addr:   %p\n", mbi->mmap_addr);
	}

	printf("Hello from the kernel!\n");

}