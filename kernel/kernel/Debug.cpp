#include <kernel/Debug.h>
#include <kernel/Device/FramebufferDevice.h>
#include <kernel/InterruptController.h>
#include <kernel/Lock/SpinLock.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Terminal/Serial.h>
#include <kernel/Terminal/TTY.h>
#include <kernel/Timer/Timer.h>

#include <LibDEFLATE/Compressor.h>
#include <LibQR/QRCode.h>

#include <ctype.h>

bool g_disable_debug = false;

namespace Debug
{

	Kernel::RecursiveSpinLock s_debug_lock;

	static constexpr char s_panic_url_prefix[] = "https://bananymous.com/panic#";
	static constexpr size_t s_qr_code_max_capacity { 2953 };
	static bool s_qr_code_shown { false };

	static char s_debug_buffer[16 * 1024] {};
	static size_t s_debug_buffer_tail { 0 };
	static size_t s_debug_buffer_size { 0 };
	static uint8_t s_debug_ansi_state { 0 };

	void dump_stack_trace()
	{
		dump_stack_trace(0, reinterpret_cast<uintptr_t>(__builtin_frame_address(0)));
	}

	void dump_stack_trace(uintptr_t ip, uintptr_t bp)
	{
		using namespace Kernel;

		struct stackframe
		{
			stackframe* bp;
			void* ip;
		};

		SpinLockGuard _(s_debug_lock);

		const stackframe* frame = reinterpret_cast<const stackframe*>(bp);

		void* first_ip = frame->ip;
		void* last_ip = 0;
		bool first = true;

		BAN::Formatter::print(Debug::putchar, "\e[36mStack trace:\r\n");

		if (ip != 0)
			BAN::Formatter::print(Debug::putchar, "    {}\r\n", reinterpret_cast<void*>(ip));

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

	static void queue_debug_buffer(char ch)
	{
		switch (s_debug_ansi_state)
		{
			case 1:
				if (ch == '[')
				{
					s_debug_ansi_state = 2;
					break;
				}
				s_debug_ansi_state = 0;
				[[fallthrough]];
			case 0:
				if (ch == '\e')
				{
					s_debug_ansi_state = 1;
					break;
				}
				if (!isprint(ch) && ch != '\n')
					break;
				s_debug_buffer[(s_debug_buffer_tail + s_debug_buffer_size) % sizeof(s_debug_buffer)] = ch;
				if (s_debug_buffer_size < sizeof(s_debug_buffer))
					s_debug_buffer_size++;
				else
					s_debug_buffer_tail = (s_debug_buffer_tail + 1) % sizeof(s_debug_buffer);
				break;
			case 2:
				if (isalpha(ch))
					s_debug_ansi_state = 0;
				break;
		}
	}

	static void reverse(char* first, char* last)
	{
		const size_t len = last - first;
		for (size_t i = 0; i < len / 2; i++)
			BAN::swap(first[i], first[len - i - 1]);
	}

	static void rotate(char* first, char* middle, char* last)
	{
		reverse(first, middle);
		reverse(middle, last);
		reverse(first, last);
	}

	static BAN::ErrorOr<BAN::Vector<uint8_t>> compress_kernel_logs()
	{
		constexpr size_t max_size = ((s_qr_code_max_capacity + 3) / 4 * 3) - sizeof(s_panic_url_prefix);

		BAN::Vector<uint8_t> result;

		size_t l = 0, r = s_debug_buffer_size;
		while (l + 50 < r)
		{
			const size_t middle = (l + r) / 2;
			const uint8_t* base = reinterpret_cast<const uint8_t*>(s_debug_buffer) + s_debug_buffer_size - middle;

			auto compressed = TRY(LibDEFLATE::Compressor({ base, middle }, LibDEFLATE::StreamType::Zlib).compress());
			if (compressed.size() > max_size)
				r = middle;
			else
			{
				l = middle;
				result = BAN::move(compressed);
			}
		}

		return result;
	}

	void dump_qr_code()
	{
		ASSERT(Kernel::g_paniced);

		auto boot_framebuffer = Kernel::FramebufferDevice::boot_framebuffer();
		if (!boot_framebuffer)
		{
			derrorln("No boot framebuffer, not generating QR code");
			return;
		}
		if (boot_framebuffer->width() < 177 + 8 || boot_framebuffer->height() < 177 + 8)
		{
			derrorln("Boot framebuffer is too small for a qr code");
			return;
		}

		// rotate logs to start from index 0 and be contiguous
		rotate(s_debug_buffer, s_debug_buffer + s_debug_buffer_tail, s_debug_buffer + sizeof(s_debug_buffer));

		auto compressed_or_error = compress_kernel_logs();
		if (compressed_or_error.is_error())
		{
			// TODO: send uncompressed logs?
			derrorln("Failed to compress kernel logs: {}", compressed_or_error.error());
			return;
		}

		auto compressed = compressed_or_error.release_value();

		static uint8_t qr_code_data[s_qr_code_max_capacity];
		size_t qr_code_data_len = 0;

		for (size_t i = 0; s_panic_url_prefix[i]; i++)
			qr_code_data[qr_code_data_len++] = s_panic_url_prefix[i];

		constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
		for (size_t i = 0; i < compressed.size() / 3; i++)
		{
			const uint32_t bits =
				((compressed[3 * i + 0]) << 16) |
				((compressed[3 * i + 1]) <<  8) |
				((compressed[3 * i + 2]) <<  0);

			qr_code_data[qr_code_data_len++] = alphabet[(bits >> 18) & 0x3F];
			qr_code_data[qr_code_data_len++] = alphabet[(bits >> 12) & 0x3F];
			qr_code_data[qr_code_data_len++] = alphabet[(bits >>  6) & 0x3F];
			qr_code_data[qr_code_data_len++] = alphabet[(bits >>  0) & 0x3F];
		}

		switch (compressed.size() % 3)
		{
			case 0:
				break;
			case 1:
			{
				const uint16_t bits =
					(compressed[compressed.size() - 1] << 4);
				qr_code_data[qr_code_data_len++] = alphabet[(bits >>  6) & 0x3F];
				qr_code_data[qr_code_data_len++] = alphabet[(bits >>  0) & 0x3F];
				break;
			}
			case 2:
			{
				const uint32_t bits =
					(compressed[compressed.size() - 2] << 10) |
					(compressed[compressed.size() - 1] <<  2);
				qr_code_data[qr_code_data_len++] = alphabet[(bits >> 12) & 0x3F];
				qr_code_data[qr_code_data_len++] = alphabet[(bits >>  6) & 0x3F];
				qr_code_data[qr_code_data_len++] = alphabet[(bits >>  0) & 0x3F];
				break;
			}
		}

		auto qr_or_error = LibQR::QRCode::generate({ qr_code_data, qr_code_data_len });
		if (qr_or_error.is_error())
		{
			derrorln("Failed to generate QR code");
			return;
		}

		auto qr_code = qr_or_error.release_value();

		// after this point no more logs are printed to framebuffer
		s_qr_code_shown = true;

		const size_t min_framebuffer_dimension = BAN::Math::min(boot_framebuffer->width(), boot_framebuffer->height());
		const size_t module_size = min_framebuffer_dimension / (qr_code.size() + 8);

		for (size_t y = 0; y < (qr_code.size() + 8) * module_size; y++)
			for (size_t x = 0; x < (qr_code.size() + 8) * module_size; x++)
				boot_framebuffer->set_pixel(x, y, 0xFFFFFF);

		for (size_t y = 0; y < qr_code.size(); y++)
		{
			for (size_t x = 0; x < qr_code.size(); x++)
			{
				if (!qr_code.get(x, y))
					continue;
				for (size_t i = 0; i < module_size; i++)
					for (size_t j = 0; j < module_size; j++)
						boot_framebuffer->set_pixel((x + 4) * module_size + j, (y + 4) * module_size + i, 0x000000);
			}
		}

		boot_framebuffer->sync_pixels_rectangle(0, 0, (qr_code.size() + 8) * module_size, (qr_code.size() + 8) * module_size);
	}

	void putchar(char ch)
	{
		using namespace Kernel;

		if (!g_paniced)
			queue_debug_buffer(ch);

		if (g_disable_debug)
			return;

		if (Kernel::Serial::has_devices())
			return Kernel::Serial::putchar_any(ch);
		if (Kernel::TTY::is_initialized())
		{
			if (s_qr_code_shown)
				return;
			return Kernel::TTY::putchar_current(ch);
		}

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
				g_terminal_driver->putchar_at(ch, col, row, TerminalColor::WHITE, TerminalColor::BLACK);

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
					g_terminal_driver->putchar_at(' ', i, row, TerminalColor::WHITE, TerminalColor::BLACK);
					if (row + 1 < g_terminal_driver->height())
						g_terminal_driver->putchar_at(' ', i, row + 1, TerminalColor::WHITE, TerminalColor::BLACK);
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
