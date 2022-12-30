#include <kernel/APIC.h>
#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/Input.h>
#include <kernel/IO.h>
#include <kernel/kmalloc.h>
#include <kernel/kprint.h>
#include <kernel/multiboot.h>
#include <kernel/Paging.h>
#include <kernel/panic.h>
#include <kernel/PIC.h>
#include <kernel/PIT.h>
#include <kernel/RTC.h>
#include <kernel/Serial.h>
#include <kernel/Shell.h>
#include <kernel/TTY.h>
#include <kernel/VESA.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")


multiboot_info_t* s_multiboot_info;

using namespace BAN;

struct ParsedCommandLine
{
	bool force_pic = false;
};

ParsedCommandLine ParseCommandLine(const char* command_line)
{
	ParsedCommandLine result;

	const char* start = command_line;
	while (true)
	{
		if (!*command_line || *command_line == ' ' || *command_line == '\t')
		{
			if (command_line - start == 6 && memcmp(start, "noapic", 6) == 0)
				result.force_pic = true;

			if (!*command_line)
				break;
			start = command_line + 1;
		}
		command_line++;
	}

	return result;
}

extern "C" void kernel_main(multiboot_info_t* mbi, uint32_t magic)
{
	DISABLE_INTERRUPTS();

	Serial::initialize();
	if (magic != 0x2BADB002)
	{
		dprintln("Invalid multiboot magic number");
		return;
	}

	Paging::Initialize();

	s_multiboot_info = mbi;

	if (!VESA::Initialize())
	{
		dprintln("Could not initialize VESA");
		return;
	}

	ParsedCommandLine cmdline;
	if (mbi->flags & 0x02)
		cmdline = ParseCommandLine((const char*)mbi->cmdline);

	APIC::Initialize(cmdline.force_pic);
	gdt_initialize();
	IDT::initialize();

	PIT::initialize();
	kmalloc_initialize();

	TTY* tty1 = new TTY;
	tty1->SetCursorPosition(0, 2);

	if (!Input::initialize())
		return;

	ENABLE_INTERRUPTS();

	kprintln("Hello from the kernel!");

	auto& shell = Kernel::Shell::Get();
	shell.SetTTY(tty1);
	shell.Run();

	for (;;)
	{
		asm("hlt");
	}
}