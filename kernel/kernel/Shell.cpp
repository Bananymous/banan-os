#include <BAN/Math.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/CPUID.h>
#include <kernel/font.h>
#include <kernel/Input.h>
#include <kernel/IO.h>
#include <kernel/PIT.h>
#include <kernel/RTC.h>
#include <kernel/Serial.h>
#include <kernel/Shell.h>
#include <kernel/TTY.h>

#define TTY_PRINT(...) Formatter::print([this](char c) { m_tty->PutChar(c); }, __VA_ARGS__)
#define TTY_PRINTLN(...) Formatter::println([this](char c) { m_tty->PutChar(c); }, __VA_ARGS__)

namespace Kernel
{
	using namespace BAN;

	static Shell* s_instance = nullptr;

	static uint8_t s_pointer[] {
		________,
		________,
		________,
		________,
		________,
		X_______,
		XX______,
		XXX_____,
		XXXX____,
		XXXXX___,
		XXXXXX__,
		XXXXXXX_,
		XXXXXXXX,
		XXX_____,
		XX______,
		X_______,
	};

	Shell& Shell::Get()
	{
		if (!s_instance)
			s_instance = new Shell();
		return *s_instance;
	}

	Shell::Shell()
	{
		Input::register_key_event_callback([](Input::KeyEvent event) { Shell::Get().KeyEventCallback(event); });
		Input::register_mouse_move_event_callback([](Input::MouseMoveEvent event) { Shell::Get().MouseMoveEventCallback(event); });
		m_buffer.Reserve(128);
	}

	void Shell::PrintPrompt()
	{
		TTY_PRINT("\e[32muser\e[m# ");
	}

	void Shell::SetTTY(TTY* tty)
	{
		m_tty = tty;
	}

	void Shell::Run()
	{
		PrintPrompt();
		for (;;)
		{
			asm volatile("hlt");
			Input::update();
		}
	}

	void Shell::ProcessCommand(const Vector<StringView>& arguments)
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
		else if (arguments.Front() == "cpuinfo")
		{
			if (arguments.Size() != 1)
			{
				TTY_PRINTLN("'cpuinfo' does not support command line arguments");
				return;	
			}
			if (!CPUID::IsAvailable())
			{
				TTY_PRINTLN("'cpuid' instruction not available");
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
			if (!CPUID::IsAvailable())
			{
				TTY_PRINTLN("'cpuid' instruction not available");
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

	static bool IsSingleUnicode(StringView sv)
	{
		if (sv.Size() == 2 && ((uint8_t)sv[0] >> 5) != 0b110)
			return false;
		if (sv.Size() == 3 && ((uint8_t)sv[0] >> 4) != 0b1110)
			return false;
		if (sv.Size() == 4 && ((uint8_t)sv[0] >> 3) != 0b11110)
			return false;
		for (uint32_t i = 1; i < sv.Size(); i++)
			if (((uint8_t)sv[i] >> 6) != 0b10)
				return false;
		return true;
	}

	static uint32_t GetLastLength(StringView sv)
	{
		if (sv.Size() < 2)
			return sv.Size();

		for (uint32_t len = 2; len <= 4; len++)
		{
			if (sv.Size() < len)
				return 1;

			if (IsSingleUnicode(sv.Substring(sv.Size() - len)))
				return len;
		}

		return 1;
	}

	void Shell::KeyEventCallback(Input::KeyEvent event)
	{
		if (!event.pressed)
			return;

		switch (event.key)
		{
			case Input::Key::Backspace:
			{
				if (!m_buffer.Empty())
				{
					TTY_PRINT("\b \b", 3);
					
					uint32_t last_len = GetLastLength(m_buffer);
					for (uint32_t i = 0; i < last_len; i++)
						m_buffer.PopBack();
				}
				break;
			}

			case Input::Key::Enter:
			case Input::Key::NumpadEnter:
			{
				TTY_PRINT("\n");
				ProcessCommand(MUST(m_buffer.SV().Split(' ')));
				m_buffer.Clear();
				PrintPrompt();
				break;
			}

			case Input::Key::Escape:
				TTY_PRINTLN("time since boot {} ms", PIT::ms_since_boot());
				break;

			case Input::Key::Tab:
				break;
			
			default:
			{
				const char* utf8 = Input::key_event_to_utf8(event);
				if (utf8)
				{
					TTY_PRINT("{}", utf8);
					m_buffer.Append(utf8);
				}
				break;
			}
		}

		if (m_mouse_pos.exists)
			VESA::PutBitmapAt(s_pointer, m_mouse_pos.x, m_mouse_pos.y, VESA::Color::BRIGHT_WHITE);
	}

	void Shell::MouseMoveEventCallback(Input::MouseMoveEvent event)
	{
		m_mouse_pos.exists = true;
		m_tty->RenderFromBuffer(m_mouse_pos.x, m_mouse_pos.y);
		m_mouse_pos.x = Math::clamp<int32_t>(m_mouse_pos.x + event.dx, 0, m_tty->Width() - 1);
		m_mouse_pos.y = Math::clamp<int32_t>(m_mouse_pos.y - event.dy, 0, m_tty->Height() - 1);
		VESA::PutBitmapAt(s_pointer, m_mouse_pos.x, m_mouse_pos.y, VESA::Color::BRIGHT_WHITE);
	}

}