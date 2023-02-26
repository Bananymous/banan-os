#include <kernel/Debug.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/IDT.h>
#include <kernel/Input.h>
#include <kernel/InterruptController.h>
#include <kernel/kmalloc.h>
#include <kernel/kprint.h>
#include <kernel/MMU.h>
#include <kernel/multiboot.h>
#include <kernel/PCI.h>
#include <kernel/PIC.h>
#include <kernel/PIT.h>
#include <kernel/Scheduler.h>
#include <kernel/Serial.h>
#include <kernel/Shell.h>
#include <kernel/TTY.h>
#include <kernel/VesaTerminalDriver.h>

#define DISABLE_INTERRUPTS() asm volatile("cli")
#define ENABLE_INTERRUPTS() asm volatile("sti")

extern "C" const char g_kernel_cmdline[];

struct ParsedCommandLine
{
	bool force_pic		= false;
	bool disable_serial	= false;
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

			if (current - start == 8 && memcmp(start, "noserial", 8) == 0)
				result.disable_serial = true;

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
	using namespace Kernel;

	DISABLE_INTERRUPTS();

	auto cmdline = ParseCommandLine();

	if (!cmdline.disable_serial)
		Serial::initialize();
	if (g_multiboot_magic != 0x2BADB002)
	{
		dprintln("Invalid multiboot magic number");
		return;
	}
	dprintln("Serial output initialized");

	kmalloc_initialize();
	dprintln("kmalloc initialized");

	IDT::initialize();
	dprintln("IDT initialized");	

	MMU::intialize();
	dprintln("MMU initialized");

	TerminalDriver* terminal_driver = VesaTerminalDriver::create();
	ASSERT(terminal_driver);
	dprintln("VESA initialized");
	TTY* tty1 = new TTY(terminal_driver);
	
	InterruptController::initialize(cmdline.force_pic);
	dprintln("Interrupt controller initialized");

	PIT::initialize();
	dprintln("PIT initialized");

	if (!PCI::initialize())
		Kernel::panic("Could not initialize PCI");
	dprintln("PCI initialized");

	if (!Input::initialize())
		dprintln("Could not initialize input drivers");
	dprintln("Input initialized");

	Scheduler::initialize();
	Scheduler& scheduler = Scheduler::get();
	MUST(scheduler.add_thread(BAN::Function<void()>(
		[terminal_driver]
		{
			if (auto error = VirtualFileSystem::initialize(); error.is_error())
			{
				derrorln("{}", error.error());
				return;
			}

			auto font_or_error = Font::load("/usr/share/fonts/zap-ext-vga16.psf");
			if (font_or_error.is_error())
				dprintln("{}", font_or_error.error());
			else
				terminal_driver->set_font(font_or_error.release_value());
		}
	)));
	MUST(scheduler.add_thread(BAN::Function<void()>(
		[tty1]
		{
			Shell* shell = new Shell(tty1);
			ASSERT(shell);
			shell->run();
		}
	)));
	scheduler.start();
	ASSERT(false);
}