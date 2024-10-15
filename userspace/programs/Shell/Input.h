#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/String.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

#include <sys/types.h>
#include <termios.h>

class Input
{
	BAN_NON_COPYABLE(Input);
	BAN_NON_MOVABLE(Input);
public:
	Input();

	BAN::Optional<BAN::String> get_input(BAN::Optional<BAN::StringView> custom_prompt);

private:
	BAN::String parse_ps1_prompt();

private:
	BAN::String m_hostname;

	BAN::Vector<BAN::String> m_buffers;
	size_t m_buffer_index { 0 };
	size_t m_buffer_col { 0 };

	BAN::Optional<ssize_t> m_tab_index;
	BAN::Optional<BAN::Vector<BAN::String>> m_tab_completions;
	size_t m_tab_completion_keep { 0 };

	int m_waiting_utf8 { 0 };
};
