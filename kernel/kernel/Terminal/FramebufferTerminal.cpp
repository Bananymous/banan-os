#include <kernel/Terminal/FramebufferTerminal.h>

namespace Kernel
{


	static consteval TerminalDriver::Palette default_palette()
	{
		TerminalDriver::Palette palette;
		palette[ 0] = 0x000000;
		palette[ 1] = 0xFF0000;
		palette[ 2] = 0x00FF00;
		palette[ 3] = 0xFFFF00;
		palette[ 4] = 0x0000FF;
		palette[ 5] = 0xFF00FF;
		palette[ 6] = 0x00FFFF;
		palette[ 7] = 0xBFBFBF;
		palette[ 8] = 0x3F3F3F;
		palette[ 9] = 0xFF7F7F;
		palette[10] = 0x7FFF7F;
		palette[11] = 0xFFFF7F;
		palette[12] = 0x7F7FFF;
		palette[13] = 0xFF7FFF;
		palette[14] = 0x7FFFFF;
		palette[15] = 0xFFFFFF;
		return palette;
	}

	BAN::ErrorOr<BAN::RefPtr<FramebufferTerminalDriver>> FramebufferTerminalDriver::create(BAN::RefPtr<FramebufferDevice> framebuffer_device)
	{
		auto* driver_ptr = new FramebufferTerminalDriver(framebuffer_device, default_palette());
		if (driver_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto driver = BAN::RefPtr<FramebufferTerminalDriver>::adopt(driver_ptr);
		TRY(driver->set_font(BAN::move(TRY(LibFont::Font::prefs()))));
		driver->set_cursor_position(0, 0);
		driver->clear(driver->m_palette[0]);
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

		m_framebuffer_device->fill(color.rgb);
		m_framebuffer_device->sync_pixels_full();

		if (m_cursor_shown)
			show_cursor(false);
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
		// NOTE: cursor is allowed to be on width as scrolling only
		//       happens after character gets printed to next line
		if (m_cursor_x == width())
			return;

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
