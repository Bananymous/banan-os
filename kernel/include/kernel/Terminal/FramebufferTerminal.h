#pragma once

#include <kernel/Device/FramebufferDevice.h>
#include <kernel/Terminal/TerminalDriver.h>

namespace Kernel
{

	class FramebufferTerminalDriver final : public TerminalDriver
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<FramebufferTerminalDriver>> create(BAN::RefPtr<FramebufferDevice>);

		virtual uint32_t width() const override { return m_framebuffer_device->width() / m_font.width(); }
		virtual uint32_t height() const override { return m_framebuffer_device->height() / m_font.height(); }

		virtual void putchar_at(uint16_t, uint32_t, uint32_t, Color, Color) override;
		virtual bool scroll(Color) override;
		virtual void clear(Color) override;

		virtual void set_cursor_position(uint32_t, uint32_t) override;

		virtual bool has_font() const override { return true; }

		virtual void set_font(const LibFont::Font& font) override { m_font = font; };
		virtual const LibFont::Font& font() const override { return m_font; };

	private:
		FramebufferTerminalDriver(BAN::RefPtr<FramebufferDevice> framebuffer_device)
			: m_framebuffer_device(framebuffer_device)
		{ }

	private:
		BAN::RefPtr<FramebufferDevice> m_framebuffer_device;
		LibFont::Font m_font;
		static constexpr Color s_cursor_color = TerminalColor::BRIGHT_WHITE;
	};

}
