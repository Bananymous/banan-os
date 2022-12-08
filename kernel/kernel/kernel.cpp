#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/Keyboard.h>
#include <kernel/kmalloc.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/PIC.h>
#include <kernel/PIT.h>
#include <kernel/tty.h>
#include <kernel/kprint.h>
#include <kernel/IO.h>

#include <string.h>
#include <stdlib.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

multiboot_info_t* s_multiboot_info;

void on_key_press(Keyboard::Key key, uint8_t modifiers, bool pressed)
{
	if (pressed)
	{
		char ascii = Keyboard::key_to_ascii(key, modifiers);
		if (ascii)
			kprint("{}", ascii);
	}
}

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
	Keyboard::initialize(on_key_press);

	kprint("Hello from the kernel!\n");

	ENABLE_INTERRUPTS();

halt:
	for (;;)
	{
		asm("hlt");
	}
}