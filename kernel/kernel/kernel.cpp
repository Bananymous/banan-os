#include <kernel/ACPI/ACPI.h>
#include <kernel/APIC.h>
#include <kernel/Arch.h>
#include <kernel/BootInfo.h>
#include <kernel/Debug.h>
#include <kernel/Device/FramebufferDevice.h>
#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/FS/ProcFS/FileSystem.h>
#include <kernel/FS/VirtualFileSystem.h>
#include <kernel/GDT.h>
#include <kernel/IDT.h>
#include <kernel/Input/PS2/Controller.h>
#include <kernel/InterruptController.h>
#include <kernel/kprint.h>
#include <kernel/Memory/Heap.h>
#include <kernel/Memory/kmalloc.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Memory/SharedMemoryObject.h>
#include <kernel/Networking/NetworkManager.h>
#include <kernel/PCI.h>
#include <kernel/PIC.h>
#include <kernel/Process.h>
#include <kernel/Processor.h>
#include <kernel/Random.h>
#include <kernel/Scheduler.h>
#include <kernel/Syscall.h>
#include <kernel/Terminal/FramebufferTerminal.h>
#include <kernel/Terminal/Serial.h>
#include <kernel/Terminal/VirtualTTY.h>
#include <kernel/Timer/Timer.h>
#include <kernel/USB/USBManager.h>

#include <LibInput/KeyboardLayout.h>

#include <ctype.h>

struct ParsedCommandLine
{
	bool force_pic       = false;
	bool disable_acpi    = false;
	bool disable_serial  = false;
	bool disable_smp     = false;
	bool disable_usb     = false;
	uint8_t ps2_override = 0;
	BAN::StringView console = "tty0"_sv;
	BAN::StringView root;
};

static bool should_disable_serial(BAN::StringView full_command_line)
{
	const char* start = full_command_line.data();
	const char* current = start;
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

extern bool g_disable_debug;
static ParsedCommandLine cmdline;

static void parse_command_line()
{
	auto full_command_line = Kernel::g_boot_info.command_line.sv();
	auto arguments = MUST(full_command_line.split(' '));
	for (auto argument : arguments)
	{
		if (argument == "noapic")
			cmdline.force_pic = true;
		else if (argument == "noserial")
			cmdline.disable_serial = true;
		else if (argument == "nosmp")
			cmdline.disable_smp = true;
		else if (argument == "nousb")
			cmdline.disable_usb = true;
		else if (argument == "noacpi")
			cmdline.disable_acpi = true;
		else if (argument == "nodebug")
			g_disable_debug = true;
		else if (argument.starts_with("ps2="))
		{
			if (argument.size() != 5 || !isdigit(argument[4]))
				dprintln("Invalid ps2= command line argument format '{}'", argument);
			cmdline.ps2_override = argument[4] - '0';
		}
		else if (argument.size() > 5 && argument.substring(0, 5) == "root=")
			cmdline.root = argument.substring(5);
		else if (argument.size() > 8 && argument.substring(0, 8) == "console=")
			cmdline.console = argument.substring(8);
	}
}

Kernel::TerminalDriver* g_terminal_driver = nullptr;

static void init2(void*);

extern "C" void kernel_main(uint32_t boot_magic, uint32_t boot_info)
{
	using namespace Kernel;

	if (!validate_boot_magic(boot_magic))
	{
		Serial::initialize();
		dprintln("Unrecognized boot magic {8H}", boot_magic);
		return;
	}

	if (!should_disable_serial(get_early_boot_command_line(boot_magic, boot_info)))
	{
		Serial::initialize();
		dprintln("Serial output initialized");
	}

	kmalloc_initialize();
	dprintln("kmalloc initialized");

	parse_boot_info(boot_magic, boot_info);
	dprintln("boot info parsed");

	Processor::create(PROCESSOR_NONE);
	Processor::initialize();
	dprintln("BSP initialized");

	PageTable::initialize_pre_heap();
	PageTable::kernel().initial_load();
	dprintln("PageTable stage1 initialized");

	Heap::initialize();
	dprintln("Heap initialzed");

	PageTable::initialize_post_heap();
	dprintln("PageTable stage2 initialized");

	parse_command_line();
	dprintln("command line parsed, root='{}', console='{}'", cmdline.root, cmdline.console);

	MUST(ACPI::ACPI::initialize());
	dprintln("ACPI initialized");

	InterruptController::initialize(cmdline.force_pic);
	dprintln("Interrupt controller initialized");

	SystemTimer::initialize(cmdline.force_pic);
	dprintln("Timers initialized");

	DevFileSystem::initialize();
	dprintln("devfs initialized");

	auto framebuffer_device = FramebufferDevice::create_from_boot_framebuffer();
	if (!framebuffer_device.is_error())
	{
		DevFileSystem::get().add_device(framebuffer_device.value());
		g_terminal_driver = FramebufferTerminalDriver::create(framebuffer_device.value());
	}
	if (g_terminal_driver)
		dprintln("Framebuffer terminal initialized");

	if (!cmdline.disable_smp)
		InterruptController::get().initialize_multiprocessor();

	ProcFileSystem::initialize();
	dprintln("procfs initialized");

	MUST(SharedMemoryObjectManager::initialize());
	dprintln("Shared memory object system initialized");

	if (Serial::has_devices())
	{
		Serial::initialize_devices();
		dprintln("Serial devices initialized");
	}

	if (g_terminal_driver)
	{
		auto vtty = MUST(VirtualTTY::create(g_terminal_driver));
		dprintln("Virtual TTY initialized");
	}

	Random::initialize();
	dprintln("RNG initialized");

	if (InterruptController::get().is_using_apic())
	{
		SystemTimer::get().dont_invoke_scheduler();
		static_cast<APIC&>(InterruptController::get()).initialize_timer();
	}

	Processor::wait_until_processors_ready();
	MUST(Processor::scheduler().initialize());

	Process::create_kernel(init2, nullptr);
	Processor::yield();

	ASSERT_NOT_REACHED();
}

static void init2(void*)
{
	using namespace Kernel;
	using namespace Kernel::Input;

	dprintln("Scheduler started");

	auto console = MUST(DevFileSystem::get().root_inode()->find_inode(cmdline.console));
	ASSERT(console->is_tty());
	static_cast<Kernel::TTY*>(console.ptr())->set_as_current();

	// This only initializes PCIManager by enumerating available devices and choosing PCIe/legacy
	// ACPI might require PCI access during its namespace initialization
	PCI::PCIManager::initialize();
	dprintln("PCI initialized");

	if (!cmdline.disable_usb)
	{
		MUST(USBManager::initialize());
		dprintln("USBManager initialized");
	}

	if (!cmdline.disable_acpi && ACPI::ACPI::get().enter_acpi_mode(InterruptController::get().is_using_apic()).is_error())
		dprintln("Failed to enter ACPI mode");

	DevFileSystem::get().initialize_device_updater();

#if 0
	dprintln("sleeping for 5 seconds");
	SystemTimer::get().sleep(5000);
#endif

	// Initialize empty keymap
	MUST(LibInput::KeyboardLayout::initialize());

	if (auto res = PS2Controller::initialize(cmdline.ps2_override); res.is_error())
		dprintln("{}", res.error());

	MUST(NetworkManager::initialize());

	// NOTE: PCI devices are the last ones to be initialized
	//       so other devices can reserve predefined interrupts
	PCI::PCIManager::get().initialize_devices(cmdline.disable_usb);
	dprintln("PCI devices initialized");

	VirtualFileSystem::initialize(cmdline.root);
	dprintln("VFS initialized");

	TTY::initialize_devices();

	MUST(Process::create_userspace({ 0, 0, 0, 0 }, "/usr/bin/init"_sv, {}));
}

extern "C" void ap_main()
{
	using namespace Kernel;

	Processor::initialize();
	PageTable::kernel().initial_load();
	InterruptController::get().enable();

	Processor::wait_until_processors_ready();
	MUST(Processor::scheduler().initialize());

	asm volatile("sti; 1: hlt; jmp 1b");
	ASSERT_NOT_REACHED();
}
