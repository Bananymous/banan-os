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
			constexpr Color()
				: rgb(0)
			{ }
			constexpr Color(uint32_t rgb)
				: rgb(rgb)
			{ }
			constexpr Color(uint8_t r, uint8_t g, uint8_t b)
				: rgb(((uint32_t)r << 16) | ((uint32_t)g << 8) | b)
			{ }
			constexpr uint8_t red() const { return (rgb >> 0x10) & 0xFF; }
			constexpr uint8_t green() const { return (rgb >> 0x08) & 0xFF; }
			constexpr uint8_t blue() const { return (rgb >> 0x00) & 0xFF; }
			uint32_t rgb;
		};

		using Palette = BAN::Array<Color, 16>;

	public:
		static BAN::ErrorOr<void> initialize_from_boot_info();
		TerminalDriver(const Palette& palette)
			: m_palette(palette)
		{}
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

		const Palette& palette() const { return m_palette; }

	protected:
		Palette m_palette;
	};

	extern BAN::RefPtr<TerminalDriver> g_terminal_driver;

	namespace TerminalColor
	{
		constexpr TerminalDriver::Color BLACK = 0x000000;
		constexpr TerminalDriver::Color WHITE = 0xFFFFFF;
	}

}
