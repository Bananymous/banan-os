#include <kernel/GDT.h>
#include <kernel/IDT.h>
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
	
	if (magic != 0x2BADB002)
		asm volatile("hlt");

	s_multiboot_info = mbi;

	terminal_initialize();

	kmalloc_initialize();

	gdt_initialize();
	idt_initialize();

	kprint("Hello from the kernel!\n");

	asm volatile("int $14");

}