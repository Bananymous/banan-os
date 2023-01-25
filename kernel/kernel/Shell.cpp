#include <BAN/Math.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/CPUID.h>
#include <kernel/Input.h>
#include <kernel/IO.h>
#include <kernel/PIT.h>
#include <kernel/RTC.h>
#include <kernel/Shell.h>

#include <ctype.h>

#define TTY_PRINT(...) Formatter::print([this](char c) { m_tty->PutChar(c); }, __VA_ARGS__)
#define TTY_PRINTLN(...) Formatter::println([this](char c) { m_tty->PutChar(c); }, __VA_ARGS__)

namespace Kernel
{
	using namespace BAN;

	static auto s_default_prompt = "\\[\e[32m\\]user\\[\e[m\\]# "_sv;

	Shell::Shell(TTY* tty)
		: m_tty(tty)
	{
		Input::register_key_event_callback({ &Shell::KeyEventCallback, this });
		SetPrompt(s_default_prompt);
		MUST(m_buffer.PushBack(""_sv));
	}

	void Shell::SetPrompt(StringView prompt)
	{
		m_prompt_length = 0;
		m_prompt = String();

		bool skipping = false;
		for (size_t i = 0; i < prompt.Size(); i++)
		{
			if (i < prompt.Size() - 1 && prompt[i] == '\\')
			{
				if (prompt[i + 1] == '[')
					skipping = true;
				if (prompt[i + 1] == ']')
					skipping = false;
				i++;
				continue;
			}
			
			MUST(m_prompt.PushBack(prompt[i]));
			if (!skipping)
				m_prompt_length++;
		}
	}

	void Shell::Run()
	{
		TTY_PRINT("{}", m_prompt);
		for (;;)
		{
			asm volatile("hlt");
			Input::update();
		}
	}

	Vector<String> Shell::ParseArguments(StringView command) const
	{
		Vector<String> result;

		while (!command.Empty())
		{
			while (!command.Empty() && isspace(command.Front()))
				command = command.Substring(1);
			
			if (command.Empty())
				break;

			MUST(result.PushBack(""_sv));

			char quoted = '\0';
			bool escape = false;
			while (!command.Empty())
			{
				char ch = command.Front();
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
								case 'f':  MUST(result.Back().PushBack('\f')); break;
								case 'n':  MUST(result.Back().PushBack('\n')); break;
								case 'r':  MUST(result.Back().PushBack('\r')); break;
								case 't':  MUST(result.Back().PushBack('\t')); break;
								case 'v':  MUST(result.Back().PushBack('\v')); break;
								case '"':  MUST(result.Back().PushBack('"'));  break;
								case '\'': MUST(result.Back().PushBack('\'')); break;
								case '\\': MUST(result.Back().PushBack('\\')); break;
								default:
									char buffer[3] { '\\', ch, '\0' };
									MUST(result.Back().Append(buffer));
									break;
							}
						}
						else
						{
							MUST(result.Back().PushBack(ch));
						}
						escape = false;
						break;
				}
				command = command.Substring(1);
			}
argument_done:
			continue;
		}

		return result;
	}

	void Shell::ProcessCommand(const Vector<String>& arguments)
	{
		if (arguments.Empty())
		{

		}
		else if (arguments.Front() == "date")
		{
			if (arguments.Size() != 1)
			{
				TTY_PRINTLN("'date' does not support command line arguments");
				return;	
			}
			auto time = RTC::GetCurrentTime();
			TTY_PRINTLN("{}", time);
		}
		else if (arguments.Front() == "echo")
		{
			if (arguments.Size() > 1)
			{
				TTY_PRINT("{}", arguments[1]);
				for (size_t i = 2; i < arguments.Size(); i++)
					TTY_PRINT(" {}", arguments[i]);
			}
			TTY_PRINTLN("");
		}
		else if (arguments.Front() == "clear")
		{
			if (arguments.Size() != 1)
			{
				TTY_PRINTLN("'clear' does not support command line arguments");
				return;	
			}
			m_tty->Clear();
			m_tty->SetCursorPosition(0, 0);
		}
		else if (arguments.Front() == "time")
		{
			auto new_args = arguments;
			new_args.Remove(0);
			auto start = PIT::ms_since_boot();
			ProcessCommand(new_args);
			auto duration = PIT::ms_since_boot() - start;
			TTY_PRINTLN("took {} ms", duration);
		}
		else if (arguments.Front() == "memory")
		{
			if (arguments.Size() != 1)
			{
				TTY_PRINTLN("'memory' does not support command line arguments");
				return;	
			}
			kmalloc_dump_info();
		}
		else if (arguments.Front() == "cpuinfo")
		{
			if (arguments.Size() != 1)
			{
				TTY_PRINTLN("'cpuinfo' does not support command line arguments");
				return;	
			}

			uint32_t ecx, edx;
			auto vendor = CPUID::GetVendor();
			CPUID::GetFeatures(ecx, edx);

			TTY_PRINTLN("Vendor: '{}'", vendor);
			TTY_PRINTLN("64-bit: {}", CPUID::Is64Bit());
			bool first = true;
			for (int i = 0; i < 32; i++)
				if (ecx & ((uint32_t)1 << i))
					TTY_PRINT("{}{}", first ? (first = false, "") : ", ", CPUID::FeatStringECX((uint32_t)1 << i));
			for (int i = 0; i < 32; i++)
				if (edx & ((uint32_t)1 << i))
					TTY_PRINT("{}{}", first ? (first = false, "") : ", ", CPUID::FeatStringEDX((uint32_t)1 << i));
			if (!first)
				TTY_PRINTLN("");
		}
		else if (arguments.Front() == "random")
		{
			if (arguments.Size() != 1)
			{
				TTY_PRINTLN("'random' does not support command line arguments");
				return;	
			}
			uint32_t ecx, edx;
			CPUID::GetFeatures(ecx, edx);
			if (!(ecx & CPUID::Features::ECX_RDRND))
			{
				TTY_PRINTLN("cpu does not support RDRAND instruction");
				return;
			}

			for (int i = 0; i < 10; i++)
			{
				uint32_t random;
				asm volatile("rdrand %0" : "=r"(random));
				TTY_PRINTLN("  0x{8H}", random);
			}
		}
		else if (arguments.Front() == "reboot")
		{
			if (arguments.Size() != 1)
			{
				TTY_PRINTLN("'reboot' does not support command line arguments");
				return;	
			}
			uint8_t good = 0x02;
			while (good & 0x02)
				good = IO::inb(0x64);
			IO::outb(0x64, 0xFE);
			asm volatile("cli; hlt");
		}
		else
		{
			TTY_PRINTLN("unrecognized command '{}'", arguments.Front());
		}

	}

	void Shell::ReRenderBuffer() const
	{
		TTY_PRINT("\e[{}G{}\e[K", m_prompt_length + 1, m_buffer[m_cursor_pos.line]);
	}

	static uint32_t GetLastLength(StringView sv)
	{
		if (sv.Size() >= 2 && ((uint8_t)sv[sv.Size() - 2] >> 5) == 0b110)	return 2;
		if (sv.Size() >= 3 && ((uint8_t)sv[sv.Size() - 3] >> 4) == 0b1110)	return 3;
		if (sv.Size() >= 4 && ((uint8_t)sv[sv.Size() - 4] >> 3) == 0b11110)	return 4;
		return Math::min<uint32_t>(sv.Size(), 1);
	}

	static uint32_t GetNextLength(StringView sv)
	{
		if (sv.Size() >= 2 && ((uint8_t)sv[0] >> 5) == 0b110)	return 2;
		if (sv.Size() >= 3 && ((uint8_t)sv[0] >> 4) == 0b1110)	return 3;
		if (sv.Size() >= 4 && ((uint8_t)sv[0] >> 3) == 0b11110)	return 4;
		return Math::min<uint32_t>(sv.Size(), 1);
	}

	static uint32_t GetUnicodeCharacterCount(StringView sv)
	{
		uint32_t len = 0;
		for (uint32_t i = 0; i < sv.Size(); i++)
		{
			uint8_t ch = sv[i];
			if ((ch >> 5) == 0b110)		i += 1;
			if ((ch >> 4) == 0b1110)	i += 2;
			if ((ch >> 3) == 0b11110)	i += 3;
			len++;
		}
		return len;
	}

	void Shell::KeyEventCallback(Input::KeyEvent event)
	{
		if (!event.pressed)
			return;

		String& current_buffer = m_buffer[m_cursor_pos.line];

		switch (event.key)
		{
			case Input::Key::Backspace:
				if (m_cursor_pos.col > 0)
				{
					TTY_PRINT("\e[D{} ", current_buffer.SV().Substring(m_cursor_pos.index));
					
					uint32_t len = GetLastLength(current_buffer.SV().Substring(0, m_cursor_pos.index));
					m_cursor_pos.index -= len;
					current_buffer.Erase(m_cursor_pos.index, len);
					m_cursor_pos.col--;
				}
				break;

			case Input::Key::Enter:
			case Input::Key::NumpadEnter:
			{
				TTY_PRINTLN("");
				auto arguments = ParseArguments(current_buffer.SV());
				if (!arguments.Empty())
				{
					ProcessCommand(arguments);
					MUST(m_old_buffer.PushBack(current_buffer));
					m_buffer = m_old_buffer;
					MUST(m_buffer.PushBack(""_sv));
					MUST(m_buffer.Back().Reserve(128));
					m_cursor_pos.line = m_buffer.Size() - 1;
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
					uint32_t len = GetLastLength(current_buffer.SV().Substring(0, m_cursor_pos.index));
					m_cursor_pos.index -= len;
					m_cursor_pos.col--;
				}
				break;

			case Input::Key::Right:
				if (m_cursor_pos.index < current_buffer.Size())
				{
					uint32_t len = GetNextLength(current_buffer.SV().Substring(m_cursor_pos.index));
					m_cursor_pos.index += len;
					m_cursor_pos.col++;
				}
				break;

			case Input::Key::Up:
				if (m_cursor_pos.line > 0)
				{
					const auto& new_buffer = m_buffer[m_cursor_pos.line - 1];
					m_cursor_pos.line--;
					m_cursor_pos.index = new_buffer.Size();
					m_cursor_pos.col = GetUnicodeCharacterCount(new_buffer);
					ReRenderBuffer();
				}
				break;

			case Input::Key::Down:
				if (m_cursor_pos.line < m_buffer.Size() - 1)
				{
					const auto& new_buffer = m_buffer[m_cursor_pos.line + 1];
					m_cursor_pos.line++;
					m_cursor_pos.index = new_buffer.Size();
					m_cursor_pos.col = GetUnicodeCharacterCount(new_buffer);
					ReRenderBuffer();
				}
				break;

			default:
			{
				const char* utf8 = Input::key_event_to_utf8(event);
				if (utf8)
				{
					TTY_PRINT("{}{}", utf8, current_buffer.SV().Substring(m_cursor_pos.index));
					MUST(current_buffer.Insert(utf8, m_cursor_pos.index));
					m_cursor_pos.index += strlen(utf8);
					m_cursor_pos.col++;
				}
				break;
			}
		}

		TTY_PRINT("\e[{}G", m_prompt_length + m_cursor_pos.col + 1);
	}

}