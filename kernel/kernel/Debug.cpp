#include <kernel/Debug.h>
#include <kernel/InterruptController.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Serial.h>
#include <kernel/SpinLock.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Timer/Timer.h>

namespace Debug
{

	void dump_stack_trace()
	{
		using namespace Kernel;

		struct stackframe
		{
			stackframe* rbp;
			uintptr_t rip;
		};

		stackframe* frame = (stackframe*)__builtin_frame_address(0);
		if (!frame)
		{
			dprintln("Could not get frame address");
			return;
		}
		uintptr_t first_rip = frame->rip;
		uintptr_t last_rip = 0;
		bool first = true;

		BAN::Formatter::print(Debug::putchar, "\e[36mStack trace:\r\n");
		while (frame)
		{
			if (PageTable::current().is_page_free((vaddr_t)frame & PAGE_ADDR_MASK))
			{
				derrorln("    {} not mapped", frame);
				break;
			}

			BAN::Formatter::print(Debug::putchar, "    {}\r\n", (void*)frame->rip);

			if (!first && frame->rip == first_rip)
			{
				derrorln("looping kernel panic :(");
				break;
			}
			else if (!first && frame->rip == last_rip)
			{
				derrorln("repeating stack trace");
				break;
			}

			last_rip = frame->rip;
			frame = frame->rbp;
			first = false;
		}
		BAN::Formatter::print(Debug::putchar, "\e[m");
	}

	void putchar(char ch)
	{
		if (Serial::is_initialized())
			return Serial::putchar(ch);
		if (Kernel::TTY::is_initialized())
			return Kernel::TTY::putchar_current(ch);
	}

	void print_prefix(const char* file, int line)
	{
		auto ms_since_boot = Kernel::SystemTimer::is_initialized() ? Kernel::SystemTimer::get().ms_since_boot() : 0;
		BAN::Formatter::print(Debug::putchar, "[{5}.{3}] {}:{}: ", ms_since_boot / 1000, ms_since_boot % 1000, file, line);
	}

	static Kernel::RecursiveSpinLock s_debug_lock;

	void DebugLock::lock()
	{
		if (interrupts_enabled())
			s_debug_lock.lock();
	}

	void DebugLock::unlock()
	{
		if (interrupts_enabled())
			s_debug_lock.unlock();
	}

}