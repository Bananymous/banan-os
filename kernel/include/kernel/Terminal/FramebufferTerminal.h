#pragma once

#include <kernel/Device/FramebufferDevice.h>
#include <kernel/Terminal/TerminalDriver.h>

namespace Kernel
{

	class FramebufferTerminalDriver final : public TerminalDriver
	{
	public:
		static FramebufferTerminalDriver* create(BAN::RefPtr<FramebufferDevice>);

		virtual uint32_t width() const override { return m_framebuffer_device->width() / font().width(); }
		virtual uint32_t height() const override { return m_framebuffer_device->height() / font().height(); }

		virtual void putchar_at(uint16_t, uint32_t, uint32_t, Color, Color) override;
		virtual bool scroll(Color) override;
		virtual void clear(Color) override;

		virtual void set_cursor_position(uint32_t, uint32_t) override;

	private:
		FramebufferTerminalDriver(BAN::RefPtr<FramebufferDevice> framebuffer_device)
			: m_framebuffer_device(framebuffer_device)
		{ }

	private:
		BAN::RefPtr<FramebufferDevice> m_framebuffer_device;
		static constexpr Color s_cursor_color = TerminalColor::BRIGHT_WHITE;
	};

}
