#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/Input.h>
#include <kernel/TTY.h>

namespace Kernel
{

	class Shell
	{
	public:
		Shell(TTY*);
		Shell(const Shell&) = delete;
		void SetPrompt(BAN::StringView);
		void Run();

	private:
		void ReRenderBuffer() const;
		BAN::Vector<BAN::String> ParseArguments(BAN::StringView) const;
		void ProcessCommand(const BAN::Vector<BAN::String>&);
		void KeyEventCallback(Input::KeyEvent);
		void MouseMoveEventCallback(Input::MouseMoveEvent);

	private:
		TTY*						m_tty;
		BAN::Vector<BAN::String>	m_old_buffer;
		BAN::Vector<BAN::String>	m_buffer;
		BAN::String					m_prompt;
		uint32_t					m_prompt_length = 0;
		
		struct
		{
			uint32_t line = 0;
			uint32_t col = 0;
			uint32_t index = 0;
		} m_cursor_pos;

		struct
		{
			bool	exists = false;
			int32_t x = 0;
			int32_t y = 0;
		} m_mouse_pos;
	};

}