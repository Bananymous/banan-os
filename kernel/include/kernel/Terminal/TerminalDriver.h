#pragma once

#include <LibFont/Font.h>

#include <stdint.h>

namespace Kernel
{

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
		TerminalDriver() : m_font(MUST(LibFont::Font::prefs())) {}
		virtual ~TerminalDriver() {}
		virtual uint32_t width() const = 0;
		virtual uint32_t height() const = 0;

		virtual void putchar_at(uint16_t, uint32_t, uint32_t, Color, Color) = 0;
		virtual bool scroll(Color) { return false; }
		virtual void clear(Color) = 0;

		virtual void set_cursor_position(uint32_t, uint32_t) = 0;

		void set_font(const LibFont::Font& font) { m_font = font; };
		const LibFont::Font& font() const { return m_font; }

	private:
		LibFont::Font m_font;
	};

	namespace TerminalColor
	{
		static constexpr TerminalDriver::Color BLACK			= 0x000000;
		static constexpr TerminalDriver::Color RED				= 0xFF0000;
		static constexpr TerminalDriver::Color GREEN			= 0x00FF00;
		static constexpr TerminalDriver::Color YELLOW			= 0xFFFF00;
		static constexpr TerminalDriver::Color BLUE				= 0x0000FF;
		static constexpr TerminalDriver::Color MAGENTA			= 0xFF00FF;
		static constexpr TerminalDriver::Color CYAN				= 0x00FFFF;
		static constexpr TerminalDriver::Color WHITE			= 0xBFBFBF;

		static constexpr TerminalDriver::Color BRIGHT_BLACK		= 0x3F3F3F;
		static constexpr TerminalDriver::Color BRIGHT_RED		= 0xFF7F7F;
		static constexpr TerminalDriver::Color BRIGHT_GREEN		= 0x7FFF7F;
		static constexpr TerminalDriver::Color BRIGHT_YELLOW	= 0xFFFF7F;
		static constexpr TerminalDriver::Color BRIGHT_BLUE		= 0x7F7FFF;
		static constexpr TerminalDriver::Color BRIGHT_MAGENTA	= 0xFF7FFF;
		static constexpr TerminalDriver::Color BRIGHT_CYAN		= 0x7FFFFF;
		static constexpr TerminalDriver::Color BRIGHT_WHITE		= 0xFFFFFF;
	}

}
