#include <kernel/Debug.h>
#include <kernel/InterruptController.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Terminal/Serial.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Timer/Timer.h>

#include <ctype.h>

bool g_disable_debug = false;

namespace Debug
{

	Kernel::RecursiveSpinLock s_debug_lock;

	void dump_stack_trace()
	{
		using namespace Kernel;

		struct stackframe
		{
			stackframe* bp;
			uintptr_t ip;
		};

		SpinLockGuard _(s_debug_lock);

		stackframe* frame = (stackframe*)__builtin_frame_address(0);
		if (!frame)
		{
			dprintln("Could not get frame address");
			return;
		}
		uintptr_t first_ip = frame->ip;
		uintptr_t last_ip = 0;
		bool first = true;

		BAN::Formatter::print(Debug::putchar, "\e[36mStack trace:\r\n");
		while (frame)
		{
			if (!PageTable::is_valid_pointer((vaddr_t)frame))
			{
				derrorln("invalid pointer {H}", (vaddr_t)frame);
				break;
			}

			if (PageTable::current().is_page_free((vaddr_t)frame & PAGE_ADDR_MASK))
			{
				derrorln("    {} not mapped", frame);
				break;
			}

			BAN::Formatter::print(Debug::putchar, "    {}\r\n", (void*)frame->ip);

			if (!first && frame->ip == first_ip)
			{
				derrorln("looping kernel panic :(");
				break;
			}
			else if (!first && frame->ip == last_ip)
			{
				derrorln("repeating stack trace");
				break;
			}

			last_ip = frame->ip;
			frame = frame->bp;
			first = false;
		}
		BAN::Formatter::print(Debug::putchar, "\e[m");
	}

	void putchar(char ch)
	{
		using namespace Kernel;

		if (g_disable_debug)
			return;

		if (Kernel::Serial::has_devices())
			return Kernel::Serial::putchar_any(ch);
		if (Kernel::TTY::is_initialized())
			return Kernel::TTY::putchar_current(ch);

		if (g_terminal_driver)
		{
			static uint32_t col = 0;
			static uint32_t row = 0;

			uint32_t row_copy = row;

			if (ch == '\n')
			{
				row++;
				col = 0;
			}
			else if (ch == '\r')
			{
				col = 0;
			}
			else
			{
				if (!isprint(ch))
					ch = '?';
				g_terminal_driver->putchar_at(ch, col, row, TerminalColor::BRIGHT_WHITE, TerminalColor::BLACK);

				col++;
				if (col >= g_terminal_driver->width())
				{
					row++;
					col = 0;
				}
			}

			if (row >= g_terminal_driver->height())
				row = 0;

			if (row != row_copy)
			{
				for (uint32_t i = col; i < g_terminal_driver->width(); i++)
				{
					g_terminal_driver->putchar_at(' ', i, row, TerminalColor::BRIGHT_WHITE, TerminalColor::BLACK);
					if (row + 1 < g_terminal_driver->height())
						g_terminal_driver->putchar_at(' ', i, row + 1, TerminalColor::BRIGHT_WHITE, TerminalColor::BLACK);
				}
			}
		}
	}

	void print_prefix(const char* file, int line)
	{
		auto ms_since_boot = Kernel::SystemTimer::is_initialized() ? Kernel::SystemTimer::get().ms_since_boot() : 0;
		BAN::Formatter::print(Debug::putchar, "[{5}.{3}] {}:{}: ", ms_since_boot / 1000, ms_since_boot % 1000, file, line);
	}

}
