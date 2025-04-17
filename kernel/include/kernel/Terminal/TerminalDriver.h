#pragma once

#include <BAN/RefPtr.h>

#include <LibFont/Font.h>

namespace Kernel
{

	class TerminalDriver : public BAN::RefCounted<TerminalDriver>
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
		static BAN::ErrorOr<void> initialize_from_boot_info();
		virtual ~TerminalDriver() {}
		virtual uint32_t width() const = 0;
		virtual uint32_t height() const = 0;

		virtual void putchar_at(uint16_t, uint32_t, uint32_t, Color, Color) = 0;
		virtual bool scroll(Color) { return false; }
		virtual void clear(Color) = 0;

		virtual void set_cursor_shown(bool) = 0;
		virtual void set_cursor_position(uint32_t, uint32_t) = 0;

		virtual bool has_font() const { return false; }
		virtual BAN::ErrorOr<void> set_font(LibFont::Font&&) { return BAN::Error::from_errno(EINVAL); }
		virtual const LibFont::Font& font() const { ASSERT_NOT_REACHED(); }
	};

	extern BAN::RefPtr<TerminalDriver> g_terminal_driver;

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
