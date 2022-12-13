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
#include <kernel/RTC.h>
#include <kernel/Serial.h>
#include <kernel/tty.h>

#include <string.h>
#include <stdlib.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

multiboot_info_t* s_multiboot_info;

char ascii_buffer[32] {};
static bool has_command(const char* cmd)
{
	size_t len = strlen(cmd);
	return memcmp(ascii_buffer + sizeof(ascii_buffer) - len - 1, cmd, len) == 0;
}
void on_key_press(Keyboard::KeyEvent event)
{
	if (event.pressed)
	{
		char ascii = Keyboard::key_event_to_ascii(event);

		if (ascii)
		{
			memmove(ascii_buffer, ascii_buffer + 1, sizeof(ascii_buffer) - 1);
			ascii_buffer[sizeof(ascii_buffer) - 1] = ascii;
			kprint("{}", ascii);
		}

		if (event.key == Keyboard::Key::Escape)
		{
			kprint("time since boot: {} ms\n", PIT::ms_since_boot());
			return;
		}
		else if (event.key == Keyboard::Key::Backspace)
		{
			memmove(ascii_buffer + 2, ascii_buffer, sizeof(ascii_buffer) - 2);
			kprint(" \b");
		}
		else if (event.key == Keyboard::Key::Enter)
		{
			if (has_command("clear"))
			{
				TTY::clear();
				TTY::set_cursor_pos(0, 0);
			}
			else if (has_command("led_disco"))
			{
				TTY::clear();
				TTY::set_cursor_pos(0, 0);
				kprintln("\e[32mLED DISCO\e[m");
				Keyboard::led_disco();
			}
			else if (has_command("reboot"))
			{
				uint8_t good = 0x02;
				while (good & 0x02)
					good = IO::inb(0x64);
				IO::outb(0x64, 0xFE);
				asm volatile("cli; hlt");
			}
		}
	}
}

extern "C"
void kernel_main(multiboot_info_t* mbi, uint32_t magic)
{
	DISABLE_INTERRUPTS();

	s_multiboot_info = mbi;

	if (magic != 0x2BADB002)
		return;

	Serial::initialize();
	TTY::initialize();

	for (int i = 30; i <= 37; i++)
		kprint("\e[{}m#", i);
	kprint("\e[m\n");

	kmalloc_initialize();

	PIC::initialize();
	gdt_initialize();
	IDT::initialize();

	PIT::initialize();
	if (!Keyboard::initialize(on_key_press))
		return;

	auto time = RTC::GetCurrentTime();
	kprintln("Today is {}", time);

	kprintln("Hello from the kernel!");

	ENABLE_INTERRUPTS();


	for (;;)
	{
		Keyboard::update_keyboard();
	}
}