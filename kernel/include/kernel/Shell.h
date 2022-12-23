#pragma once

#include <BAN/String.h>
#include <kernel/Keyboard.h>
#include <kernel/TTY.h>

namespace Kernel
{

	class Shell
	{
	public:
		Shell(const Shell&) = delete;

		static Shell& Get();

		void SetTTY(TTY* tty);

		void Run();

	private:
		Shell();
		void PrintPrompt();
		void ProcessCommand(const BAN::Vector<BAN::StringView>&);
		void KeyEventCallback(Keyboard::KeyEvent);

	private:
		TTY*		m_tty;
		BAN::String	m_buffer;
	};

}