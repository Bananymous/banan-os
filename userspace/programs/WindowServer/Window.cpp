#include "Window.h"

#include <BAN/Debug.h>
#include <BAN/ScopeGuard.h>

#include <LibGUI/Window.h>

#include <sys/banan-os.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

Window::~Window()
{
	munmap(m_fb_addr, client_width() * client_height() * 4);
	smo_delete(m_smo_key);

	LibGUI::EventPacket::DestroyWindowEvent packet;
	(void)packet.send_serialized(m_client_fd);
	close(m_client_fd);
}

BAN::ErrorOr<void> Window::initialize(BAN::StringView title, uint32_t width, uint32_t height)
{
	m_title.clear();
	TRY(m_title.append(title));
	TRY(resize(width, height));
	return {};
}

BAN::ErrorOr<void> Window::resize(uint32_t width, uint32_t height)
{
	const size_t fb_bytes = width * height * 4;

	long smo_key = smo_create(fb_bytes, PROT_READ | PROT_WRITE);
	if (smo_key == -1)
		return BAN::Error::from_errno(errno);
	BAN::ScopeGuard smo_deleter([&]() { smo_delete(smo_key); });

	uint32_t* fb_addr = static_cast<uint32_t*>(smo_map(smo_key));
	if (fb_addr == nullptr)
		return BAN::Error::from_errno(errno);
	memset(fb_addr, 0xFF, fb_bytes);
	BAN::ScopeGuard smo_unmapper([&]() { munmap(fb_addr, fb_bytes); });

	{
		const auto old_area = m_client_area;

		m_client_area.width = width;
		m_client_area.height = height;
		auto title_bar_ret = prepare_title_bar();
		m_client_area = old_area;

		if (title_bar_ret.is_error())
			return title_bar_ret.release_error();
	}

	smo_deleter.disable();
	smo_unmapper.disable();

	if (m_fb_addr)
		munmap(m_fb_addr, client_width() * client_height() * 4);
	if (m_smo_key)
		smo_delete(m_smo_key);

	m_fb_addr = fb_addr;
	m_smo_key = smo_key;
	m_client_area.width = width;
	m_client_area.height = height;

	return {};
}

BAN::ErrorOr<void> Window::prepare_title_bar()
{
	const uint32_t font_w = m_font.width();
	const uint32_t font_h = m_font.height();
	const uint32_t font_p = m_font.pitch();

	TRY(m_title_bar_data.resize(title_bar_width() * title_bar_height()));
	for (auto& pixel : m_title_bar_data)
		pixel = 0xFFFFFFFF;

	const auto text_area = title_text_area();

	for (size_t i = 0; i < m_title.size() && (i + 1) * font_w < static_cast<uint32_t>(text_area.width); i++)
	{
		const auto* glyph = m_font.glyph(m_title[i]);
		if (glyph == nullptr)
			continue;

		const int32_t y_off = (font_h < (uint32_t)title_bar_height()) ? (title_bar_height() - font_h) / 2 : 0;
		const int32_t x_off = y_off + i * font_w;
		for (int32_t y = 0; (uint32_t)y < font_h; y++)
		{
			if (y + y_off >= title_bar_height())
				break;
			for (int32_t x = 0; (uint32_t)x < font_w; x++)
			{
				if (x + x_off >= text_area.width)
					break;
				const uint8_t bitmask = 1 << (font_w - x - 1);
				if (glyph[y * font_p] & bitmask)
					m_title_bar_data[(y_off + y) * title_bar_width() + (x_off + x)] = 0xFF000000;
			}
		}
	}

	return {};
}
