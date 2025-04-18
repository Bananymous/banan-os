#include <kernel/Terminal/FramebufferTerminal.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<FramebufferTerminalDriver>> FramebufferTerminalDriver::create(BAN::RefPtr<FramebufferDevice> framebuffer_device)
	{
		auto* driver_ptr = new FramebufferTerminalDriver(framebuffer_device);
		if (driver_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto driver = BAN::RefPtr<FramebufferTerminalDriver>::adopt(driver_ptr);
		TRY(driver->set_font(BAN::move(TRY(LibFont::Font::prefs()))));
		driver->set_cursor_position(0, 0);
		driver->clear(TerminalColor::BLACK);
		return driver;
	}

	void FramebufferTerminalDriver::putchar_at(uint16_t ch, uint32_t x, uint32_t y, Color fg, Color bg)
	{
		const uint8_t* glyph = m_font.has_glyph(ch) ? m_font.glyph(ch) : m_font.glyph('?');

		const bool update_cursor = m_cursor_shown && x == m_cursor_x && y == m_cursor_y;

		x *= m_font.width();
		y *= m_font.height();

		for (uint32_t dy = 0; dy < m_font.height() && y + dy < m_framebuffer_device->height(); dy++)
		{
			for (uint32_t dx = 0; dx < m_font.width() && x + dx < m_framebuffer_device->width(); dx++)
			{
				const uint8_t bitmask = 1 << (m_font.width() - dx - 1);
				const auto color = glyph[dy * m_font.pitch()] & bitmask ? fg : bg;
				m_framebuffer_device->set_pixel(x + dx, y + dy, color.rgb);
			}
		}

		m_framebuffer_device->sync_pixels_rectangle(x, y, m_font.width(), m_font.height());

		if (update_cursor)
			show_cursor(false);
	}

	bool FramebufferTerminalDriver::scroll(Color color)
	{
		if (m_cursor_shown)
			show_cursor(true);
		m_framebuffer_device->scroll(m_font.height(), color.rgb);
		m_framebuffer_device->sync_pixels_full();
		if (m_cursor_shown)
			show_cursor(false);
		return true;
	}

	void FramebufferTerminalDriver::clear(Color color)
	{
		for (auto& pixel : m_cursor_data)
			pixel = color.rgb;

		for (uint32_t y = 0; y < m_framebuffer_device->height(); y++)
			for (uint32_t x = 0; x < m_framebuffer_device->width(); x++)
				m_framebuffer_device->set_pixel(x, y, color.rgb);
		m_framebuffer_device->sync_pixels_full();
	}

	void FramebufferTerminalDriver::read_cursor()
	{
		const uint32_t cursor_h = m_font.height() / 8;
		const uint32_t cursor_w = m_font.width();
		const uint32_t cursor_top = m_font.height() * 13 / 16;

		const uint32_t x = m_cursor_x * m_font.width();
		const uint32_t y = m_cursor_y * m_font.height();

		for (uint32_t dy = 0; dy < cursor_h; dy++)
			for (uint32_t dx = 0; dx < cursor_w; dx++)
				m_cursor_data[dy * cursor_w + dx] = m_framebuffer_device->get_pixel(x + dx, y + cursor_top + dy);
	}

	void FramebufferTerminalDriver::show_cursor(bool use_data)
	{
		if (!use_data)
			read_cursor();

		const uint32_t cursor_h = m_font.height() / 8;
		const uint32_t cursor_w = m_font.width();
		const uint32_t cursor_top = m_font.height() * 13 / 16;

		const uint32_t x = m_cursor_x * m_font.width();
		const uint32_t y = m_cursor_y * m_font.height();

		const auto get_color =
			[&](uint32_t x, uint32_t y) -> uint32_t
			{
				if (!use_data)
					return m_cursor_color.rgb;
				return m_cursor_data[y * cursor_w + x];
			};

		for (uint32_t dy = 0; dy < cursor_h; dy++)
			for (uint32_t dx = 0; dx < cursor_w; dx++)
				m_framebuffer_device->set_pixel(x + dx, y + cursor_top + dy, get_color(dx, dy));
		m_framebuffer_device->sync_pixels_rectangle(x, y + cursor_top, cursor_w, cursor_h);
	}

	void FramebufferTerminalDriver::set_cursor_shown(bool shown)
	{
		if (m_cursor_shown == shown)
			return;
		m_cursor_shown = shown;
		show_cursor(!m_cursor_shown);
	}

	void FramebufferTerminalDriver::set_cursor_position(uint32_t x, uint32_t y)
	{
		if (x == m_cursor_x && y == m_cursor_y)
			return;

		if (!m_cursor_shown)
		{
			m_cursor_x = x;
			m_cursor_y = y;
			return;
		}

		show_cursor(true);
		m_cursor_x = x;
		m_cursor_y = y;
		show_cursor(false);
	}

	BAN::ErrorOr<void> FramebufferTerminalDriver::set_font(LibFont::Font&& font)
	{
		const uint32_t cursor_h = font.height() / 8;
		const uint32_t cursor_w = font.width();
		TRY(m_cursor_data.resize(cursor_h * cursor_w));
		for (auto& val : m_cursor_data)
			val = TerminalColor::BLACK.rgb;

		m_font = BAN::move(font);
		m_cursor_x = BAN::Math::clamp<uint32_t>(m_cursor_x, 0, width() - 1);
		m_cursor_y = BAN::Math::clamp<uint32_t>(m_cursor_y, 0, height() - 1);
		return {};
	}

}
