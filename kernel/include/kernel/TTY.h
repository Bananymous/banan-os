#pragma once

#include <kernel/VESA.h>
#include <kernel/Serial.h>

class TTY
{
public:
	TTY();
	void Clear();
	void PutChar(char ch);
	void Write(const char* data, size_t size);
	void WriteString(const char* data);
	void SetCursorPos(int x, int y);

	static void PutCharCurrent(char ch);

private:
	void ResetAnsiEscape();
	void HandleAnsiSGR();
	void HandleAnsiEscape(uint16_t ch);

private:
	struct Cell
	{
		VESA::Color foreground = VESA::Color::BRIGHT_WHITE;
		VESA::Color background = VESA::Color::BLACK;
		uint8_t		character = ' ';
	};

	struct AnsiState
	{
		uint8_t mode	= '\0';
		int32_t index	= 0;
		int32_t nums[2]	= { -1, -1 };
	};

	uint32_t	m_width			{ 0 };
	uint32_t	m_height		{ 0 };
	uint32_t	m_row			{ 0 };
	uint32_t	m_column		{ 0 };
	VESA::Color	m_foreground	{ VESA::Color::BRIGHT_WHITE};
	VESA::Color	m_background	{ VESA::Color::BLACK };
	Cell*		m_buffer		{ nullptr };
	AnsiState	m_ansi_state;
};
