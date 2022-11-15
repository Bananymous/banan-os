#include <kernel/GDT.h>
#include <kernel/kmalloc.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/tty.h>
#include <kernel/kprint.h>

#include <string.h>
#include <stdlib.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

multiboot_info_t* s_multiboot_info;

extern "C"
void kernel_main(multiboot_info_t* mbi, uint32_t magic)
{
	DISABLE_INTERRUPTS();

	s_multiboot_info = mbi;

	terminal_initialize();

	if (magic != 0x2BADB002)
		Kernel::panic("Invalid magic in multiboot");

	if (!(mbi->flags & (1 << 6)))
		Kernel::panic("Bootloader did not provide memory map");

	for (uint32_t i = 0; i < mbi->mmap_length;)
	{
		multiboot_memory_map_t* mmmt = (multiboot_memory_map_t*)(mbi->mmap_addr + i);
		if (mmmt->type == 1)
			kprint("Size: {}, Addr: {}, Length: {}, Type: {}\n", mmmt->size, (void*)mmmt->base_addr, (void*)mmmt->length, mmmt->type);

		i += mmmt->size + sizeof(uint32_t);
	}

	printf("Hello from the kernel!\n");

	kprint("Hello from the kernel!\n");
}