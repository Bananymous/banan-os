#pragma once

#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/Input/KeyEvent.h>

namespace Kernel
{

	class Shell
	{
	public:
		Shell();
		Shell(const Shell&) = delete;
		BAN::ErrorOr<void> set_prompt(BAN::StringView);
		void run();

	private:
		void rerender_buffer() const;
		BAN::Vector<BAN::String> parse_arguments(BAN::StringView) const;
		BAN::ErrorOr<void> process_command(const BAN::Vector<BAN::String>&);
		BAN::ErrorOr<void> update_prompt();

	private:
		BAN::Vector<BAN::String>	m_old_buffer;
		BAN::Vector<BAN::String>	m_buffer;
		BAN::String					m_prompt_string;
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