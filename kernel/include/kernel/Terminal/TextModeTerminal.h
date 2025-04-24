#pragma once

#include <kernel/Terminal/TerminalDriver.h>

namespace Kernel
{

	class TextModeTerminalDriver final : public TerminalDriver
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<TextModeTerminalDriver>> create_from_boot_info();
		~TextModeTerminalDriver();

		uint32_t width() const override { return m_width; }
		uint32_t height() const override { return m_height; }

		void putchar_at(uint16_t, uint32_t, uint32_t, Color, Color) override;
		void clear(Color) override;

		void set_cursor_shown(bool) override;
		void set_cursor_position(uint32_t, uint32_t) override;

	private:
		TextModeTerminalDriver(paddr_t paddr, uint32_t width, uint32_t height, uint32_t pitch, const Palette& palette)
			: TerminalDriver(palette)
			, m_paddr(paddr)
			, m_width(width)
			, m_height(height)
			, m_pitch(pitch)
		{}

		BAN::ErrorOr<void> initialize();
		void move_cursor_impl(uint32_t x, uint32_t y);

	private:
		const paddr_t m_paddr;
		const uint32_t m_width;
		const uint32_t m_height;
		const uint32_t m_pitch;
		vaddr_t m_vaddr { 0 };

		uint32_t m_cursor_x { 0 };
		uint32_t m_cursor_y { 0 };
		bool m_cursor_shown { true };
		static constexpr Color s_cursor_color = TerminalColor::WHITE;
	};

}
