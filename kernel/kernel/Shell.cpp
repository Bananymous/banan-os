#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/IO.h>
#include <kernel/Keyboard.h>
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

	void Shell::ProcessCommand(BAN::StringView command)
	{
		auto arguments = command.Split(' ');
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
				ProcessCommand(m_buffer);
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