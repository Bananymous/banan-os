#include <kernel/Debug.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/GDT.h>
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

struct Test
{
	Test()							{ dprintln("construct (default)"); }
	Test(const Test&)				{ dprintln("construct (copy)"); }
	Test(Test&&)					{ dprintln("construct (move)"); }
	~Test()							{ dprintln("destruct"); }
	Test& operator=(const Test&)	{ dprintln("assign (copy)"); return *this; }
	Test& operator=(Test&&)			{ dprintln("assign (move)"); return *this; }
};

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const Test& test, const ValueFormat& format)
	{
		print_argument(putc, &test, format);
	}

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

	GDT::initialize();
	dprintln("GDT initialized");

	IDT::initialize();
	dprintln("IDT initialized");	

	MMU::intialize();
	dprintln("MMU initialized");

	PCI::initialize();
	dprintln("PCI initialized");

	TerminalDriver* terminal_driver = VesaTerminalDriver::create();
	ASSERT(terminal_driver);
	dprintln("VESA initialized");
	TTY* tty1 = new TTY(terminal_driver);
	
	InterruptController::initialize(cmdline.force_pic);
	dprintln("Interrupt controller initialized");

	PIT::initialize();
	dprintln("PIT initialized");

	if (!Input::initialize())
		dprintln("Could not initialize input drivers");
	dprintln("Input initialized");

	MUST(Scheduler::initialize());
	Scheduler& scheduler = Scheduler::get();
	MUST(scheduler.add_thread(MUST(Thread::create(
		[](void* terminal_driver)
		{
			MUST(VirtualFileSystem::initialize());

			auto font_or_error = Font::load("/usr/share/fonts/zap-ext-vga16.psf");
			if (font_or_error.is_error())
				dprintln("{}", font_or_error.error());
			else
				((TerminalDriver*)terminal_driver)->set_font(font_or_error.release_value());
		}, terminal_driver
	))));
	MUST(scheduler.add_thread(MUST(Thread::create(
		[](void* tty)
		{
			Shell* shell = new Shell((TTY*)tty);
			ASSERT(shell);
			shell->run();
		}, tty1
	))));
	scheduler.start();
	ASSERT(false);
}