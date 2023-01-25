#include <kernel/Debug.h>
#include <kernel/IDT.h>
#include <kernel/Input.h>
#include <kernel/InterruptController.h>
#include <kernel/kmalloc.h>
#include <kernel/kprint.h>
#include <kernel/MMU.h>
#include <kernel/multiboot.h>
#include <kernel/PIC.h>
#include <kernel/PIT.h>
#include <kernel/Serial.h>
#include <kernel/Shell.h>
#include <kernel/TTY.h>
#include <kernel/VesaTerminalDriver.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

extern "C" const char g_kernel_cmdline[];

using namespace BAN;

struct ParsedCommandLine
{
	bool force_pic = false;
};

ParsedCommandLine ParseCommandLine()
{
	ParsedCommandLine result;

	if (!(g_multiboot_info->flags & 0x02))
		return result;

	const char* start = g_kernel_cmdline;
	const char* current = g_kernel_cmdline;
	while (true)
	{
		if (!*current || *current == ' ' || *current == '\t')
		{
			if (current - start == 6 && memcmp(start, "noapic", 6) == 0)
				result.force_pic = true;

			if (!*current)
				break;
			start = current + 1;
		}
		current++;
	}

	return result;
}

extern "C" void kernel_main()
{
	DISABLE_INTERRUPTS();

	Serial::initialize();
	if (g_multiboot_magic != 0x2BADB002)
	{
		dprintln("Invalid multiboot magic number");
		return;
	}
	dprintln("Serial output initialized");

	auto cmdline = ParseCommandLine();

	kmalloc_initialize();
	dprintln("kmalloc initialized");

	IDT::initialize();
	dprintln("IDT initialized");

	MMU::Intialize();
	dprintln("MMU initialized");

	TerminalDriver* terminal_driver = VesaTerminalDriver::Create();
	ASSERT(terminal_driver);
	dprintln("VESA initialized");
	TTY* tty1 = new TTY(terminal_driver);
	
	InterruptController::Initialize(cmdline.force_pic);
	dprintln("Interrupt controller initialized");
	
	PIT::initialize();
	dprintln("PIT initialized");
	if (!Input::initialize())
		return;
	dprintln("8042 initialized");

	ENABLE_INTERRUPTS();

	kprintln("Hello from the kernel!");

	Kernel::Shell shell(tty1);
	shell.Run();

	for (;;)
	{
		asm("hlt");
	}
}