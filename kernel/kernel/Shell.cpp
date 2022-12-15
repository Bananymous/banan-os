#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/CPUID.h>
#include <kernel/IO.h>
#include <kernel/Keyboard.h>
#include <kernel/PIT.h>
#include <kernel/RTC.h>
#include <kernel/Shell.h>
#include <kernel/tty.h>

namespace Kernel
{

	static Shell* s_instance = nullptr;

	Shell& Shell::Get()
	{
		if (!s_instance)
			s_instance = new Shell();
		return *s_instance;
	}

	Shell::Shell()
	{
		Keyboard::register_key_event_callback([](Keyboard::KeyEvent event) { Shell::Get().KeyEventCallback(event); });
		m_buffer.Reserve(128);
	}

	void Shell::PrintPrompt()
	{
		kprint("\e[32muser\e[m# ");
	}

	void Shell::Run()
	{
		PrintPrompt();
		for (;;)
		{
			asm volatile("hlt");
			Keyboard::update_keyboard();
		}
	}

	void Shell::ProcessCommand(const BAN::Vector<BAN::StringView>& arguments)
	{
		if (arguments.Empty())
			return;

		if (arguments.Front() == "date")
		{
			if (arguments.Size() != 1)
			{
				kprintln("'date' does not support command line arguments");
				return;	
			}
			auto time = RTC::GetCurrentTime();
			kprintln("{}", time);
			return;
		}
		
		if (arguments.Front() == "echo")
		{
			if (arguments.Size() > 1)
			{
				kprint("{}", arguments[1]);
				for (size_t i = 2; i < arguments.Size(); i++)
					kprint(" {}", arguments[i]);
			}
			kprintln();
			return;
		}

		if (arguments.Front() == "clear")
		{
			if (arguments.Size() != 1)
			{
				kprintln("'clear' does not support command line arguments");
				return;	
			}
			TTY::clear();
			TTY::set_cursor_pos(0, 0);
			return;
		}

		if (arguments.Front() == "time")
		{
			auto new_args = arguments;
			new_args.Remove(0);
			auto start = PIT::ms_since_boot();
			ProcessCommand(new_args);
			auto duration = PIT::ms_since_boot() - start;
			kprintln("took {} ms", duration);
			return;
		}

		if (arguments.Front() == "cpuinfo")
		{
			if (arguments.Size() != 1)
			{
				kprintln("'cpuinfo' does not support command line arguments");
				return;	
			}
			if (!CPUID::IsAvailable())
			{
				kprintln("'cpuid' instruction not available");
				return;
			}

			uint32_t ecx, edx;
			auto vendor = CPUID::GetVendor();
			CPUID::GetFeatures(ecx, edx);

			kprintln("Vendor: '{}'", vendor);
			bool first = true;
			for (int i = 0; i < 32; i++)
				if (ecx & ((uint32_t)1 << i))
					kprint("{}{}", first ? (first = false, "") : ", ", CPUID::FeatStringECX((uint32_t)1 << i));
			for (int i = 0; i < 32; i++)
				if (edx & ((uint32_t)1 << i))
					kprint("{}{}", first ? (first = false, "") : ", ", CPUID::FeatStringEDX((uint32_t)1 << i));
			if (!first)
				kprintln();
			
			return;
		}

		if (arguments.Front() == "random")
		{
			if (arguments.Size() != 1)
			{
				kprintln("'random' does not support command line arguments");
				return;	
			}
			if (!CPUID::IsAvailable())
			{
				kprintln("'cpuid' instruction not available");
				return;
			}
			uint32_t ecx, edx;
			CPUID::GetFeatures(ecx, edx);
			if (!(ecx & CPUID::Features::ECX_RDRND))
			{
				kprintln("cpu does not support RDRAND instruction");
				return;
			}

			for (int i = 0; i < 10; i++)
			{
				uint32_t random;
				asm volatile("rdrand %0" : "=r"(random));
				kprintln("  0x{8H}", random);
			}

			return;
		}

		if (arguments.Front() == "reboot")
		{
			if (arguments.Size() != 1)
			{
				kprintln("'reboot' does not support command line arguments");
				return;	
			}
			uint8_t good = 0x02;
			while (good & 0x02)
				good = IO::inb(0x64);
			IO::outb(0x64, 0xFE);
			asm volatile("cli; hlt");
			return;
		}

		kprintln("unrecognized command '{}'", arguments.Front());
	}

	void Shell::KeyEventCallback(Keyboard::KeyEvent event)
	{
		if (!event.pressed)
			return;

		switch (event.key)
		{
			case Keyboard::Key::Backspace:
			{
				if (!m_buffer.Empty())
				{
					kprint("\b \b", 3);
					m_buffer.PopBack();
				}
				break;
			}

			case Keyboard::Key::Enter:
			case Keyboard::Key::NumpadEnter:
			{
				kprint("\n");
				ProcessCommand(m_buffer.SV().Split(' '));
				m_buffer.Clear();
				PrintPrompt();
				break;
			}

			case Keyboard::Key::Tab:
				event.key = Keyboard::Key::Space;
				// fall through
			
			default:
			{
				char ascii = Keyboard::key_event_to_ascii(event);
				if (ascii)
				{
					kprint("{}", ascii);
					m_buffer.PushBack(ascii);
				}
				break;
			}
		}
	}


}