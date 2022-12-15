#pragma once

#include <BAN/String.h>
#include <kernel/Keyboard.h>

namespace Kernel
{

	class Shell
	{
	public:
		Shell(const Shell&) = delete;

		static Shell& Get();

		void Run();

	private:
		Shell();
		void PrintPrompt();
		void ProcessCommand(const BAN::Vector<BAN::StringView>&);
		void KeyEventCallback(Keyboard::KeyEvent);

	private:
		BAN::String m_buffer;
	};

}