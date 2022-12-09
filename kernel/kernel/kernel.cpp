#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/Keyboard.h>
#include <kernel/kmalloc.h>
#include <kernel/kprint.h>
#include <kernel/multiboot.h>
#include <kernel/panic.h>
#include <kernel/PIC.h>
#include <kernel/PIT.h>
#include <kernel/Serial.h>
#include <kernel/tty.h>

#include <string.h>
#include <stdlib.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

multiboot_info_t* s_multiboot_info;

void on_key_press(Keyboard::Key key, uint8_t modifiers, bool pressed)
{
	if (pressed)
	{
		if (key == Keyboard::Key::Escape)
		{
			kprint("time since boot: {} ms\n", PIT::ms_since_boot());
			return;
		}

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
		return;

	if (mbi->framebuffer.type != 2)
		return;

	TTY::initialize();
	Serial::initialize();

	kmalloc_initialize();

	PIC::initialize();
	gdt_initialize();
	IDT::initialize();

	PIT::initialize();
	Keyboard::initialize(on_key_press);


	kprintln("Hello from the kernel!");

	dprintln("Hello emulator from kernel!");

	int** lol = new int*[10];
	for (int i = 0; i < 10; i++)
		lol[i] = new int;

	kprint("{.2}\n", -12.123f);
	kprint("0x{.H}", 0xcafebabe);

	for (;;)
	{
		asm("hlt");
	}
}