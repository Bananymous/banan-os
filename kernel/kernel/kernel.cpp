#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/kmalloc.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/PIC.h>
#include <kernel/PIT.h>
#include <kernel/PS2.h>
#include <kernel/tty.h>
#include <kernel/kprint.h>
#include <kernel/IO.h>

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

	if (magic != 0x2BADB002)
		goto halt;

	terminal_initialize();

	kmalloc_initialize();

	PIC::initialize();
	gdt_initialize();
	IDT::initialize();

	PIT::initialize();
	PS2::initialize();

	kprint("Hello from the kernel!\n");

	ENABLE_INTERRUPTS();

halt:
	for (;;)
	{
		asm("hlt");
	}
}