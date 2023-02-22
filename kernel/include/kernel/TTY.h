#pragma once

#include <kernel/TerminalDriver.h>
#include <kernel/SpinLock.h>

class TTY
{
public:
	TTY(TerminalDriver*);
	void clear();
	void putchar(char ch);
	void write(const char* data, size_t size);
	void write_string(const char* data);
	void set_cursor_position(uint32_t x, uint32_t y);
	void set_font(const Kernel::Font&);

	uint32_t height() const { return m_height; }
	uint32_t width() const { return m_width; }

	void render_from_buffer(uint32_t x, uint32_t y);

	// for kprint
	static void putchar_current(char ch);
	static bool is_initialized();

private:
	void reset_ansi_escape();
	void handle_ansi_sgr();
	void handle_ansi_escape(uint16_t ch);
	void putchar_at(uint16_t ch, uint32_t x, uint32_t y);

private:
	struct Cell
	{
		TerminalDriver::Color foreground = TerminalColor::BRIGHT_WHITE;
		TerminalDriver::Color background = TerminalColor::BLACK;
		uint16_t	character = ' ';
	};

	struct AnsiState
	{
		uint8_t mode	= '\0';
		int32_t index	= 0;
		int32_t nums[2]	= { -1, -1 };
	};

	uint32_t				m_width			{ 0 };
	uint32_t				m_height		{ 0 };
	uint32_t				m_row			{ 0 };
	uint32_t				m_column		{ 0 };
	TerminalDriver::Color	m_foreground	{ TerminalColor::BRIGHT_WHITE };
	TerminalDriver::Color	m_background	{ TerminalColor::BLACK };
	Cell*					m_buffer		{ nullptr };
	AnsiState				m_ansi_state;
	TerminalDriver*			m_terminal_driver { nullptr };
	Kernel::SpinLock		m_lock;
};
