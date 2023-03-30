#include <kernel/ACPI.h>
#include <kernel/Arch.h>
#include <kernel/Debug.h>
#include <kernel/DeviceManager.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/Input/PS2Controller.h>
#include <kernel/InterruptController.h>
#include <kernel/kmalloc.h>
#include <kernel/kprint.h>
#include <kernel/MMU.h>
#include <kernel/multiboot.h>
#include <kernel/PCI.h>
#include <kernel/PIC.h>
#include <kernel/PIT.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Serial.h>
#include <kernel/Shell.h>
#include <kernel/Syscall.h>
#include <kernel/TTY.h>
#include <kernel/VesaTerminalDriver.h>

extern "C" const char g_kernel_cmdline[];

struct ParsedCommandLine
{
	bool force_pic		= false;
	bool disable_serial	= false;
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
	}
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

extern "C" uintptr_t g_rodata_start;
extern "C" uintptr_t g_rodata_end;

extern "C" uintptr_t g_userspace_start;
extern "C" uintptr_t g_userspace_end;

extern void userspace_entry();

static void init2(void*);

extern "C" void kernel_main()
{
	using namespace Kernel;

	DISABLE_INTERRUPTS();

	if (!should_disable_serial())
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

	parse_command_line();
	dprintln("command line parsed, root='{}'", cmdline.root);

	PCI::initialize();
	dprintln("PCI initialized");

	TerminalDriver* terminal_driver = VesaTerminalDriver::create();
	ASSERT(terminal_driver);
	dprintln("VESA initialized");
	TTY* tty1 = new TTY(terminal_driver);
	ASSERT(tty1);
	
	MUST(ACPI::initialize());
	dprintln("ACPI initialized");

	InterruptController::initialize(cmdline.force_pic);
	dprintln("Interrupt controller initialized");

	PIT::initialize();
	dprintln("PIT initialized");

	MUST(Scheduler::initialize());
	Scheduler& scheduler = Scheduler::get();
#if 0 
	MUST(scheduler.add_thread(MUST(Thread::create(
		[] (void*)
		{
			MMU::get().allocate_range((uintptr_t)&g_userspace_start, (uintptr_t)&g_userspace_end - (uintptr_t)&g_userspace_start, MMU::Flags::UserSupervisor | MMU::Flags::Present);
			MMU::get().allocate_range((uintptr_t)&g_rodata_start,    (uintptr_t)&g_rodata_end    - (uintptr_t)&g_rodata_start,    MMU::Flags::UserSupervisor | MMU::Flags::Present);

			void* userspace_stack = kmalloc(4096, 4096);
			ASSERT(userspace_stack);
			MMU::get().allocate_page((uintptr_t)userspace_stack, MMU::Flags::UserSupervisor | MMU::Flags::ReadWrite | MMU::Flags::Present);

			BOCHS_BREAK();

#if ARCH(x86_64)
			asm volatile(
				"pushq %0;"
				"pushq %1;"
				"pushfq;"
				"pushq %2;"
				"pushq %3;"
				"iretq;"
				:: "r"((uintptr_t)0x20 | 3), "r"((uintptr_t)userspace_stack + 4096), "r"((uintptr_t)0x18 | 3), "r"(userspace_entry)
			);
#else
			asm volatile(
				"movl %0, %%eax;"
				"movw %%ax, %%ds;"
				"movw %%ax, %%es;"
				"movw %%ax, %%fs;"
				"movw %%ax, %%gs;"

				"movl %1, %%esp;"
				"pushl %0;"
				"pushl %1;"
				"pushfl;"
				"pushl %2;"
				"pushl %3;"
				"iret;"
				:: "r"((uintptr_t)0x20 | 3), "r"((uintptr_t)userspace_stack + 4096), "r"((uintptr_t)0x18 | 3), "r"(userspace_entry)
			);
#endif
		}
	))));
#else
	MUST(scheduler.add_thread(MUST(Thread::create(init2, tty1))));
#endif
	scheduler.start();
	ASSERT(false);
}

static void init2(void* tty1_ptr)
{
	using namespace Kernel;
	using namespace Kernel::Input;

	TTY* tty1 = (TTY*)tty1_ptr;

	DeviceManager::initialize();

	MUST(VirtualFileSystem::initialize(cmdline.root));

	if (auto res = PS2Controller::initialize(); res.is_error())
		dprintln("{}", res.error());

	MUST(Process::create_kernel(
		[](void* tty1) 
		{
			Shell* shell = new Shell((TTY*)tty1);
			ASSERT(shell);
			shell->run();
		}, tty1
	));
}