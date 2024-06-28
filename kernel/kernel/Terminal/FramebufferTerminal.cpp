#include <kernel/Terminal/FramebufferTerminal.h>

namespace Kernel
{

	FramebufferTerminalDriver* FramebufferTerminalDriver::create(BAN::RefPtr<FramebufferDevice> framebuffer_device)
	{
		auto* driver = new FramebufferTerminalDriver(framebuffer_device);
		if (driver == nullptr)
			return nullptr;
		driver->set_cursor_position(0, 0);
		driver->clear(TerminalColor::BLACK);
		return driver;
	}

	void FramebufferTerminalDriver::putchar_at(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		const uint8_t* glyph = font().has_glyph(ch) ? font().glyph(ch) : font().glyph('?');

		x *= font().width();
		y *= font().height();

		for (uint32_t dy = 0; dy < font().height() && y + dy < m_framebuffer_device->height(); dy++)
		{
			for (uint32_t dx = 0; dx < font().width() && x + dx < m_framebuffer_device->width(); dx++)
			{
				const uint8_t bitmask = 1 << (font().width() - dx - 1);
				const auto color = glyph[dy * font().pitch()] & bitmask ? fg : bg;
				m_framebuffer_device->set_pixel(x + dx, y + dy, color.rgb);
			}
		}

		m_framebuffer_device->sync_pixels_rectangle(x, y, font().width(), font().height());
	}

	bool FramebufferTerminalDriver::scroll(Color color)
	{
		m_framebuffer_device->scroll(font().height(), color.rgb);
		m_framebuffer_device->sync_pixels_full();
		return true;
	}

	void FramebufferTerminalDriver::clear(Color color)
	{
		for (uint32_t y = 0; y < m_framebuffer_device->height(); y++)
			for (uint32_t x = 0; x < m_framebuffer_device->width(); x++)
				m_framebuffer_device->set_pixel(x, y, color.rgb);
		m_framebuffer_device->sync_pixels_full();
	}

	void FramebufferTerminalDriver::set_cursor_position(uint32_t x, uint32_t y)
	{
		const uint32_t cursor_h = font().height() / 8;
		const uint32_t cursor_top = font().height() * 13 / 16;

		x *= font().width();
		y *= font().height();

		for (uint32_t dy = 0; dy < cursor_h; dy++)
			for (uint32_t dx = 0; dx < font().width(); dx++)
				m_framebuffer_device->set_pixel(x + dx, y + cursor_top + dy, s_cursor_color.rgb);
		m_framebuffer_device->sync_pixels_rectangle(x, y + cursor_top, font().width(), cursor_h);
	}

}
