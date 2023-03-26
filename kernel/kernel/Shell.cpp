#include <BAN/Math.h>
#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/CPUID.h>
#include <kernel/Input.h>
#include <kernel/IO.h>
#include <kernel/PIT.h>
#include <kernel/PCI.h>
#include <kernel/Process.h>
#include <kernel/RTC.h>
#include <kernel/Shell.h>

#include <fcntl.h>
#include <ctype.h>

#define TTY_PRINT(...) BAN::Formatter::print([this](char c) { m_tty->putchar(c); }, __VA_ARGS__)
#define TTY_PRINTLN(...) BAN::Formatter::println([this](char c) { m_tty->putchar(c); }, __VA_ARGS__)

namespace Kernel
{

	static auto s_default_prompt = "\\[\e[32m\\]user\\[\e[m\\]:\\[\e[34m\\]\\w\\[\e[m\\]# "sv;

	Shell::Shell(TTY* tty)
		: m_tty(tty)
	{
		Input::register_key_event_callback({ &Shell::key_event_callback, this });
		MUST(set_prompt(s_default_prompt));
		MUST(m_buffer.push_back(""sv));
	}

	BAN::ErrorOr<void> Shell::set_prompt(BAN::StringView prompt)
	{
		m_prompt_string = prompt;
		TRY(update_prompt());
		return {};
	}

	BAN::ErrorOr<void> Shell::update_prompt()
	{
		m_prompt_length = 0;
		m_prompt.clear();

		bool skipping = false;
		for (size_t i = 0; i < m_prompt_string.size(); i++)
		{
			if (i < m_prompt_string.size() - 1 && m_prompt_string[i] == '\\')
			{
				switch (m_prompt_string[i + 1])
				{
					case '[':
						skipping = true;
						break;
					case ']':
						skipping = false;
						break;
					case 'w':
					{
						auto working_directory = Process::current()->working_directory();
						TRY(m_prompt.append(working_directory));
						m_prompt_length += working_directory.size();
						break;
					}
					default:
						dprintln("unknown escape character '{}' in shell prompt", m_prompt_string[i + 1]);
						break;
				}
				i++;
				continue;
			}
			
			TRY(m_prompt.push_back(m_prompt_string[i]));
			if (!skipping)
				m_prompt_length++;
		}

		return {};
	}

	void Shell::run()
	{
		TTY_PRINT("{}", m_prompt);
		for (;;)
		{
			PIT::sleep(1); // sleep until next reschedule
			Input::update();
		}
	}

	BAN::Vector<BAN::String> Shell::parse_arguments(BAN::StringView command) const
	{
		BAN::Vector<BAN::String> result;

		while (!command.empty())
		{
			while (!command.empty() && isspace(command.front()))
				command = command.substring(1);
			
			if (command.empty())
				break;

			MUST(result.push_back(""sv));

			char quoted = '\0';
			bool escape = false;
			while (!command.empty())
			{
				char ch = command.front();
				switch (ch)
				{
					case '"':
					case '\'':
						if (!quoted)
							quoted = ch;
						else if (ch == quoted)
							quoted = '\0';
						else
							goto default_case;
						break;
					case '\\':
						if (escape)
							goto default_case;
						escape = true;
						break;
					default:
default_case:
						if (isspace(ch) && !quoted && !escape)
							goto argument_done;
						if (quoted && escape)
						{
							switch (ch)
							{
								case 'f':  MUST(result.back().push_back('\f')); break;
								case 'n':  MUST(result.back().push_back('\n')); break;
								case 'r':  MUST(result.back().push_back('\r')); break;
								case 't':  MUST(result.back().push_back('\t')); break;
								case 'v':  MUST(result.back().push_back('\v')); break;
								case '"':  MUST(result.back().push_back('"'));  break;
								case '\'': MUST(result.back().push_back('\'')); break;
								case '\\': MUST(result.back().push_back('\\')); break;
								default:
									char buffer[3] { '\\', ch, '\0' };
									MUST(result.back().append(buffer));
									break;
							}
						}
						else
						{
							MUST(result.back().push_back(ch));
						}
						escape = false;
						break;
				}
				command = command.substring(1);
			}
argument_done:
			continue;
		}

		return result;
	}

	static uint32_t const crc32_table[256] =
	{
		0x00000000,
		0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
		0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6,
		0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
		0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac,
		0x5bd4b01b, 0x569796c2, 0x52568b75, 0x6a1936c8, 0x6ed82b7f,
		0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a,
		0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
		0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58,
		0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033,
		0xa4ad16ea, 0xa06c0b5d, 0xd4326d90, 0xd0f37027, 0xddb056fe,
		0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
		0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4,
		0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
		0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5,
		0x2ac12072, 0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
		0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca, 0x7897ab07,
		0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c,
		0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1,
		0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
		0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b,
		0xbb60adfc, 0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698,
		0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d,
		0x94ea7b2a, 0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
		0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2, 0xc6bcf05f,
		0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
		0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80,
		0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
		0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a,
		0x58c1663d, 0x558240e4, 0x51435d53, 0x251d3b9e, 0x21dc2629,
		0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c,
		0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
		0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e,
		0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65,
		0xeba91bbc, 0xef68060b, 0xd727bbb6, 0xd3e6a601, 0xdea580d8,
		0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
		0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2,
		0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
		0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74,
		0x857130c3, 0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
		0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c, 0x7b827d21,
		0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a,
		0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e, 0x18197087,
		0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
		0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d,
		0x2056cd3a, 0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce,
		0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb,
		0xdbee767c, 0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
		0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4, 0x89b8fd09,
		0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
		0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf,
		0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
	};

	BAN::ErrorOr<void> Shell::process_command(const BAN::Vector<BAN::String>& arguments)
	{
		if (arguments.empty())
		{

		}
		else if (arguments.front() == "date")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'date' does not support command line arguments");
			auto time = RTC::get_current_time();
			TTY_PRINTLN("{}", time);
		}
		else if (arguments.front() == "echo")
		{
			if (arguments.size() > 1)
			{
				TTY_PRINT("{}", arguments[1]);
				for (size_t i = 2; i < arguments.size(); i++)
					TTY_PRINT(" {}", arguments[i]);
			}
			TTY_PRINTLN("");
		}
		else if (arguments.front() == "clear")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'clear' does not support command line arguments");
			m_tty->clear();
			m_tty->set_cursor_position(0, 0);
		}
		else if (arguments.front() == "time")
		{
			auto new_args = arguments;
			new_args.remove(0);
			auto start = PIT::ms_since_boot();
			TRY(process_command(new_args));
			auto duration = PIT::ms_since_boot() - start;
			TTY_PRINTLN("took {} ms", duration);
		}
		else if (arguments.front() == "thread")
		{
			struct thread_data_t
			{
				Shell* shell;
				SpinLock& lock;
				const BAN::Vector<BAN::String>& arguments;
			};
			
			auto function = [](void* data)
			{
				thread_data_t* thread_data = (thread_data_t*)data;
				Shell* shell = thread_data->shell;
				auto args = thread_data->arguments;
				thread_data->lock.unlock();

				args.remove(0);
				PIT::sleep(5000);

				if (auto res = shell->process_command(args); res.is_error())
					BAN::Formatter::println([&](char c) { shell->m_tty->putchar(c); }, "{}", res.error());
			};

			SpinLock spinlock;
			thread_data_t thread_data = { this, spinlock, arguments };
			spinlock.lock();
			TRY(Process::current()->add_thread(function, &thread_data));
			while (spinlock.is_locked());
		}
		else if (arguments.front() == "memory")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'memory' does not support command line arguments");
			kmalloc_dump_info();
		}
		else if (arguments.front() == "sleep")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'sleep' does not support command line arguments");
			PIT::sleep(5000);
		}
		else if (arguments.front() == "cpuinfo")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'cpuinfo' does not support command line arguments");

			uint32_t ecx, edx;
			auto vendor = CPUID::get_vendor();
			CPUID::get_features(ecx, edx);

			TTY_PRINTLN("Vendor: '{}'", vendor);
			TTY_PRINTLN("64-bit: {}", CPUID::is_64_bit());
			bool first = true;
			for (int i = 0; i < 32; i++)
				if (ecx & ((uint32_t)1 << i))
					TTY_PRINT("{}{}", first ? (first = false, "") : ", ", CPUID::feature_string_ecx((uint32_t)1 << i));
			for (int i = 0; i < 32; i++)
				if (edx & ((uint32_t)1 << i))
					TTY_PRINT("{}{}", first ? (first = false, "") : ", ", CPUID::feature_string_edx((uint32_t)1 << i));
			if (!first)
				TTY_PRINTLN("");
		}
		else if (arguments.front() == "random")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'random' does not support command line arguments");
			uint32_t ecx, edx;
			CPUID::get_features(ecx, edx);
			if (!(ecx & CPUID::Features::ECX_RDRND))
				return BAN::Error::from_c_string("cpu does not support RDRAND instruction");

			for (int i = 0; i < 10; i++)
			{
				uint32_t random;
				asm volatile("rdrand %0" : "=r"(random));
				TTY_PRINTLN("  0x{8H}", random);
			}
		}
		else if (arguments.front() == "reboot")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'reboot' does not support command line arguments");
			uint8_t good = 0x02;
			while (good & 0x02)
				good = IO::inb(0x64);
			IO::outb(0x64, 0xFE);
			asm volatile("cli; hlt");
		}
		else if (arguments.front() == "lspci")
		{
			if (arguments.size() != 1)
				return BAN::Error::from_c_string("'lspci' does not support command line arguments");
			for (auto& device : PCI::get().devices())
				TTY_PRINTLN("{2H}:{2H}.{2H} {2H}", device.bus(), device.dev(), device.func(), device.class_code());
		}
		else if (arguments.front() == "ls")
		{
			if (arguments.size() > 2)
				return BAN::Error::from_c_string("usage: 'ls [path]'");

			auto mode_string = [](mode_t mode)
			{
				static char buffer[11] {};
				buffer[0] = (mode & Inode::Mode::IFDIR) ? 'd' : '-';
				buffer[1] = (mode & Inode::Mode::IRUSR) ? 'r' : '-';
				buffer[2] = (mode & Inode::Mode::IWUSR) ? 'w' : '-';
				buffer[3] = (mode & Inode::Mode::IXUSR) ? 'x' : '-';
				buffer[4] = (mode & Inode::Mode::IRGRP) ? 'r' : '-';
				buffer[5] = (mode & Inode::Mode::IWGRP) ? 'w' : '-';
				buffer[6] = (mode & Inode::Mode::IXGRP) ? 'x' : '-';
				buffer[7] = (mode & Inode::Mode::IROTH) ? 'r' : '-';
				buffer[8] = (mode & Inode::Mode::IWOTH) ? 'w' : '-';
				buffer[9] = (mode & Inode::Mode::IXOTH) ? 'x' : '-';
				return (const char*)buffer;
			};

			BAN::String path = (arguments.size() == 2) ? arguments[1] : Process::current()->working_directory();

			int fd = TRY(Process::current()->open(path, O_RDONLY));
			BAN::ScopeGuard _([fd] { MUST(Process::current()->close(fd)); });

			BAN::Vector<BAN::String> all_entries;

			BAN::Vector<BAN::String> entries;
			while (!(entries = TRY(Process::current()->read_directory_entries(fd))).empty())
			{
				TRY(all_entries.reserve(all_entries.size() + entries.size()));
				for (auto& entry : entries)
					TRY(all_entries.push_back(entry));
			}

			BAN::String entry_prefix;
			TRY(entry_prefix.append(path));
			TRY(entry_prefix.push_back('/'));
			for (const auto& entry : all_entries)
			{
				stat st;

				BAN::String entry_path;
				TRY(entry_path.append(entry_prefix));
				TRY(entry_path.append(entry));
				TRY(Process::current()->stat(entry_path, &st));

				const char* color =
					(st.st_mode & Inode::Mode::IFDIR) ? "34" :
					(st.st_mode & Inode::Mode::IXUSR) ? "32" :
														"";
				
				TTY_PRINTLN("  {} {7} \e[{}m{}\e[m", mode_string(st.st_mode), st.st_size, color, entry);
			}
		}
		else if (arguments.front() == "cat")
		{
			if (arguments.size() != 2)
				return BAN::Error::from_c_string("usage: 'cat path'");
			
			int fd = TRY(Process::current()->open(arguments[1], O_RDONLY));
			BAN::ScopeGuard _([fd] { MUST(Process::current()->close(fd)); });

			char buffer[1024] {};
			while (true)
			{
				size_t n_read = TRY(Process::current()->read(fd, buffer, sizeof(buffer)));
				if (n_read == 0)
					break;
				TTY_PRINT("{}", BAN::StringView(buffer, n_read));
			}
			TTY_PRINTLN("");
		}
		else if (arguments.front() == "cd")
		{
			if (arguments.size() > 2)
				return BAN::Error::from_c_string("usage 'cd path'");
			BAN::StringView path = arguments.size() == 2 ? arguments[1].sv() : "/"sv;
			TRY(Process::current()->set_working_directory(path));
			TRY(update_prompt());
		}
		else if (arguments.front() == "touch")
		{
			if (arguments.size() != 2)
				return BAN::Error::from_c_string("usage 'touch path'");
			TRY(Process::current()->creat(arguments[1], 0));
		}
		else if (arguments.front() == "cksum")
		{
			if (arguments.size() < 2)
				return BAN::Error::from_c_string("usage 'cksum paths...'");

			uint8_t buffer[1024];
			for (size_t i = 1; i < arguments.size(); i++)
			{
				int fd = TRY(Process::current()->open(arguments[i], O_RDONLY));
				BAN::ScopeGuard _([fd] { MUST(Process::current()->close(fd)); });

				uint32_t crc32 = 0;
				uint32_t total_read = 0;

				while (true)
				{
					size_t n_read = TRY(Process::current()->read(fd, buffer, sizeof(buffer)));
					if (n_read == 0)
						break;
					for (size_t j = 0; j < n_read; j++)
        				crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ buffer[j]) & 0xFF];
					total_read += n_read;
				}

				for (uint32_t length = total_read; length; length >>= 8)
					crc32 = (crc32 << 8) ^ crc32_table[((crc32 >> 24) ^ length) & 0xFF];
				crc32 = ~crc32 & 0xFFFFFFFF;

				TTY_PRINTLN("{} {} {}", crc32, total_read, arguments[i]);
			}
		}
		else if (arguments.front() == "loadfont")
		{
			if (arguments.size() != 2)
				return BAN::Error::from_c_string("usage: 'loadfont font_path'");

			auto font = TRY(Font::load(arguments[1]));
			m_tty->set_font(font);
		}
		else
		{
			return BAN::Error::from_format("unrecognized command '{}'", arguments.front());
		}

		return {};
	}

	void Shell::rerender_buffer() const
	{
		TTY_PRINT("\e[{}G{}\e[K", m_prompt_length + 1, m_buffer[m_cursor_pos.line]);
	}

	static uint32_t get_last_length(BAN::StringView sv)
	{
		if (sv.size() >= 2 && ((uint8_t)sv[sv.size() - 2] >> 5) == 0b110)	return 2;
		if (sv.size() >= 3 && ((uint8_t)sv[sv.size() - 3] >> 4) == 0b1110)	return 3;
		if (sv.size() >= 4 && ((uint8_t)sv[sv.size() - 4] >> 3) == 0b11110)	return 4;
		return BAN::Math::min<uint32_t>(sv.size(), 1);
	}

	static uint32_t get_next_length(BAN::StringView sv)
	{
		if (sv.size() >= 2 && ((uint8_t)sv[0] >> 5) == 0b110)	return 2;
		if (sv.size() >= 3 && ((uint8_t)sv[0] >> 4) == 0b1110)	return 3;
		if (sv.size() >= 4 && ((uint8_t)sv[0] >> 3) == 0b11110)	return 4;
		return BAN::Math::min<uint32_t>(sv.size(), 1);
	}

	static uint32_t get_unicode_character_count(BAN::StringView sv)
	{
		uint32_t len = 0;
		for (uint32_t i = 0; i < sv.size(); i++)
		{
			uint8_t ch = sv[i];
			if ((ch >> 5) == 0b110)		i += 1;
			if ((ch >> 4) == 0b1110)	i += 2;
			if ((ch >> 3) == 0b11110)	i += 3;
			len++;
		}
		return len;
	}

	void Shell::key_event_callback(Input::KeyEvent event)
	{
		if (!event.pressed)
			return;

		BAN::String& current_buffer = m_buffer[m_cursor_pos.line];

		switch (event.key)
		{
			case Input::Key::Backspace:
				if (m_cursor_pos.col > 0)
				{
					TTY_PRINT("\e[D{} ", current_buffer.sv().substring(m_cursor_pos.index));
					
					uint32_t len = get_last_length(current_buffer.sv().substring(0, m_cursor_pos.index));
					m_cursor_pos.index -= len;
					current_buffer.erase(m_cursor_pos.index, len);
					m_cursor_pos.col--;
				}
				break;

			case Input::Key::Enter:
			case Input::Key::NumpadEnter:
			{
				TTY_PRINTLN("");
				auto arguments = parse_arguments(current_buffer.sv());
				if (!arguments.empty())
				{
					if (auto res = process_command(arguments); res.is_error())
						TTY_PRINTLN("{}", res.error());
					MUST(m_old_buffer.push_back(current_buffer));
					m_buffer = m_old_buffer;
					MUST(m_buffer.push_back(""sv));
					m_cursor_pos.line = m_buffer.size() - 1;
				}
				m_cursor_pos.col = 0;
				m_cursor_pos.index = 0;
				TTY_PRINT("{}", m_prompt);
				break;
			}

			case Input::Key::Escape:
				TTY_PRINTLN("time since boot {} ms", PIT::ms_since_boot());
				break;

			case Input::Key::Tab:
				break;
			
			case Input::Key::Left:
				if (m_cursor_pos.index > 0)
				{					
					uint32_t len = get_last_length(current_buffer.sv().substring(0, m_cursor_pos.index));
					m_cursor_pos.index -= len;
					m_cursor_pos.col--;
				}
				break;

			case Input::Key::Right:
				if (m_cursor_pos.index < current_buffer.size())
				{
					uint32_t len = get_next_length(current_buffer.sv().substring(m_cursor_pos.index));
					m_cursor_pos.index += len;
					m_cursor_pos.col++;
				}
				break;

			case Input::Key::Up:
				if (m_cursor_pos.line > 0)
				{
					const auto& new_buffer = m_buffer[m_cursor_pos.line - 1];
					m_cursor_pos.line--;
					m_cursor_pos.index = new_buffer.size();
					m_cursor_pos.col = get_unicode_character_count(new_buffer);
					rerender_buffer();
				}
				break;

			case Input::Key::Down:
				if (m_cursor_pos.line < m_buffer.size() - 1)
				{
					const auto& new_buffer = m_buffer[m_cursor_pos.line + 1];
					m_cursor_pos.line++;
					m_cursor_pos.index = new_buffer.size();
					m_cursor_pos.col = get_unicode_character_count(new_buffer);
					rerender_buffer();
				}
				break;

			case Input::Key::A:
				if (event.modifiers & 2)
				{
					m_cursor_pos.col = m_cursor_pos.index = 0;
					break;
				}
				// fall through

			default:
			{
				const char* utf8 = Input::key_event_to_utf8(event);
				if (utf8)
				{
					TTY_PRINT("{}{}", utf8, current_buffer.sv().substring(m_cursor_pos.index));
					MUST(current_buffer.insert(utf8, m_cursor_pos.index));
					m_cursor_pos.index += strlen(utf8);
					m_cursor_pos.col++;
				}
				break;
			}
		}

		TTY_PRINT("\e[{}G", (m_prompt_length + m_cursor_pos.col) % m_tty->width() + 1);
	}

}