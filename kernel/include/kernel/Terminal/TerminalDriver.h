#pragma once

#include <kernel/Font.h>

#include <stdint.h>

class TerminalDriver
{
public:
	struct Color
	{
		constexpr Color(uint32_t rgb)
			: rgb(rgb)
		{ }
		constexpr Color(uint8_t r, uint8_t g, uint8_t b)
			: rgb(((uint32_t)r << 16) | ((uint32_t)g << 8) | b)
		{ }
		uint8_t red()	const { return (rgb >> 0x10) & 0xFF; }
		uint8_t green() const { return (rgb >> 0x08) & 0xFF; }
		uint8_t blue()	const { return (rgb >> 0x00) & 0xFF; }
		uint32_t rgb;
	};

public:
	TerminalDriver() : m_font(MUST(Kernel::Font::prefs())) {}
	virtual ~TerminalDriver() {}
	virtual uint32_t width() const = 0;
	virtual uint32_t height() const = 0;

	virtual void putchar_at(uint16_t, uint32_t, uint32_t, Color, Color) = 0;
	virtual void clear(Color) = 0;

	virtual void set_cursor_position(uint32_t, uint32_t) = 0;

	void set_font(const Kernel::Font& font) { m_font = font; };
	const Kernel::Font& font() const { return m_font; }

private:
	Kernel::Font m_font;
};

namespace TerminalColor
{
	static constexpr TerminalDriver::Color BLACK			= 0x000000;
	static constexpr TerminalDriver::Color BLUE				= 0x0000AA;
	static constexpr TerminalDriver::Color GREEN			= 0x00AA00;
	static constexpr TerminalDriver::Color CYAN				= 0x00AAAA;
	static constexpr TerminalDriver::Color RED				= 0xAA0000;
	static constexpr TerminalDriver::Color MAGENTA			= 0xAA00AA;
	static constexpr TerminalDriver::Color YELLOW			= 0xAA5500;
	static constexpr TerminalDriver::Color WHITE			= 0xAAAAAA;

	static constexpr TerminalDriver::Color BRIGHT_BLACK		= 0x555555;
	static constexpr TerminalDriver::Color BRIGHT_BLUE		= 0x5555FF;
	static constexpr TerminalDriver::Color BRIGHT_GREEN		= 0x55FF55;
	static constexpr TerminalDriver::Color BRIGHT_CYAN		= 0x55FFFF;
	static constexpr TerminalDriver::Color BRIGHT_RED		= 0xFF5555;
	static constexpr TerminalDriver::Color BRIGHT_MAGENTA	= 0xFF55FF;
	static constexpr TerminalDriver::Color BRIGHT_YELLOW	= 0xFFFF55;
	static constexpr TerminalDriver::Color BRIGHT_WHITE		= 0xFFFFFF;
}