#pragma once

#include <kernel/Device/FramebufferDevice.h>
#include <kernel/Terminal/TerminalDriver.h>

namespace Kernel
{

	class FramebufferTerminalDriver final : public TerminalDriver
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<FramebufferTerminalDriver>> create(BAN::RefPtr<FramebufferDevice>);

		uint32_t width() const override { return m_framebuffer_device->width() / m_font.width(); }
		uint32_t height() const override { return m_framebuffer_device->height() / m_font.height(); }

		void putchar_at(uint16_t, uint32_t, uint32_t, Color, Color) override;
		bool scroll(Color) override;
		void clear(Color) override;

		void set_cursor_shown(bool) override;
		void set_cursor_position(uint32_t, uint32_t) override;

		bool has_font() const override { return true; }

		BAN::ErrorOr<void> set_font(LibFont::Font&& font) override;
		const LibFont::Font& font() const override { return m_font; };

	private:
		FramebufferTerminalDriver(BAN::RefPtr<FramebufferDevice> framebuffer_device, const Palette& palette)
			: TerminalDriver(palette)
			, m_framebuffer_device(framebuffer_device)
		{}

		void read_cursor();
		void show_cursor(bool use_data);

	private:
		BAN::RefPtr<FramebufferDevice> m_framebuffer_device;
		LibFont::Font m_font;

		uint32_t m_cursor_x { 0 };
		uint32_t m_cursor_y { 0 };
		bool m_cursor_shown { true };
		BAN::Vector<uint32_t> m_cursor_data;
		static constexpr Color m_cursor_color = TerminalColor::WHITE;
	};

}
