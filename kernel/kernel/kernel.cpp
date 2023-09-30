#include <kernel/ACPI.h>
#include <kernel/Arch.h>
#include <kernel/Debug.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/Input/PS2Controller.h>
#include <kernel/InterruptController.h>
#include <kernel/kprint.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/multiboot.h>
#include <kernel/PCI.h>
#include <kernel/PIC.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Syscall.h>
#include <kernel/Terminal/Serial.h>
#include <kernel/Terminal/VirtualTTY.h>
#include <kernel/Terminal/VesaTerminalDriver.h>
#include <kernel/Timer/Timer.h>

extern "C" const char g_kernel_cmdline[];

struct ParsedCommandLine
{
	bool force_pic		= false;
	bool disable_serial	= false;
	BAN::StringView console = "tty0"sv;
	BAN::StringView root;
};

static bool should_disable_serial()
{
	if (!(g_multiboot_info->flags & 0x02))
		return false;

	const char* start = g_kernel_cmdline;
	const char* current = g_kernel_cmdline;
	while (true)
	{
		if (!*current || *current == ' ' || *current == '\t')
		{
			if (current - start == 8 && memcmp(start, "noserial", 8) == 0)
				return true;
			if (!*current)
				break;
			start = current + 1;
		}
		current++;
	}

	return false;
}

static ParsedCommandLine cmdline;

static void parse_command_line()
{
	if (!(g_multiboot_info->flags & 0x02))
		return;

	BAN::StringView full_command_line(g_kernel_cmdline);
	auto arguments = MUST(full_command_line.split(' '));

	for (auto argument : arguments)
	{
		if (argument == "noapic")
			cmdline.force_pic = true;
		else if (argument == "noserial")
			cmdline.disable_serial = true;
		else if (argument.size() > 5 && argument.substring(0, 5) == "root=")
			cmdline.root = argument.substring(5);
		else if (argument.size() > 8 && argument.substring(0, 8) == "console=")
			cmdline.console = argument.substring(8);
	}
}

extern "C" uint8_t g_userspace_start[];
extern "C" uint8_t g_userspace_end[];

static void init2(void*);

extern "C" void kernel_main()
{
	using namespace Kernel;

	DISABLE_INTERRUPTS();

	if (!should_disable_serial())
	{
		Serial::initialize();
		dprintln("Serial output initialized");
	}

	if (g_multiboot_magic != 0x2BADB002)
	{
		dprintln("Invalid multiboot magic number");
		return;
	}

	kmalloc_initialize();
	dprintln("kmalloc initialized");

	GDT::initialize();
	dprintln("GDT initialized");

	IDT::initialize();
	dprintln("IDT initialized");

	PageTable::initialize();
	dprintln("PageTable initialized");

	Heap::initialize();
	dprintln("Heap initialzed");

	parse_command_line();
	dprintln("command line parsed, root='{}', console='{}'", cmdline.root, cmdline.console);

	MUST(ACPI::initialize());
	dprintln("ACPI initialized");

	InterruptController::initialize(cmdline.force_pic);
	dprintln("Interrupt controller initialized");

	SystemTimer::initialize(cmdline.force_pic);
	dprintln("Timers initialized");

	DevFileSystem::initialize();
	dprintln("devfs initialized");

	ProcFileSystem::initialize();
	dprintln("procfs initialized");

	if (Serial::has_devices())
	{
		Serial::initialize_devices();
		dprintln("Serial devices initialized");
	}

	TerminalDriver* terminal_driver = VesaTerminalDriver::create();
	ASSERT(terminal_driver);
	dprintln("VESA initialized");

	auto vtty = MUST(VirtualTTY::create(terminal_driver));
	dprintln("Virtual TTY initialized");

	MUST(Scheduler::initialize());
	dprintln("Scheduler initialized");

	Scheduler& scheduler = Scheduler::get();
	Process::create_kernel(init2, nullptr);
	scheduler.start();

	ASSERT_NOT_REACHED();
}

static void init2(void*)
{
	using namespace Kernel;
	using namespace Kernel::Input;

	dprintln("Scheduler started");

	InterruptController::get().enter_acpi_mode();

	auto console = MUST(DevFileSystem::get().root_inode()->find_inode(cmdline.console));
	ASSERT(console->is_tty());
	((TTY*)console.ptr())->set_as_current();

	DevFileSystem::get().initialize_device_updater();

	PCI::PCIManager::initialize();
	dprintln("PCI initialized");

	VirtualFileSystem::initialize(cmdline.root);

	if (auto res = PS2Controller::initialize(); res.is_error())
		dprintln("{}", res.error());

	TTY::initialize_devices();

	MUST(Process::create_userspace({ 0, 0, 0, 0 }, "/usr/bin/init"sv));
}
