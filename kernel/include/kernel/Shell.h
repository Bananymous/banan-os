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
		void set_prompt(BAN::StringView);
		void run();

	private:
		void rerender_buffer() const;
		BAN::Vector<BAN::String> parse_arguments(BAN::StringView) const;
		void process_command(const BAN::Vector<BAN::String>&);
		void key_event_callback(Input::KeyEvent);

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
	};

}