#include "Window.h"

#include <BAN/Debug.h>

#include <LibGUI/Window.h>

#include <sys/banan-os.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

Window::Window(int fd, Rectangle area, long smo_key, BAN::StringView title, const LibFont::Font& font)
	: m_client_fd(fd)
	, m_client_area(area)
	, m_smo_key(smo_key)
{
	MUST(m_title.append(title));
	prepare_title_bar(font);

	m_fb_addr = static_cast<uint32_t*>(smo_map(smo_key));
	ASSERT(m_fb_addr);
	memset(m_fb_addr, 0, client_width() * client_height() * 4);
}

Window::~Window()
{
	munmap(m_fb_addr, client_width() * client_height() * 4);
	smo_delete(m_smo_key);

	LibGUI::EventPacket::DestroyWindowEvent packet;
	(void)packet.send_serialized(m_client_fd);
	close(m_client_fd);
}

void Window::prepare_title_bar(const LibFont::Font& font)
{
	const size_t title_bar_bytes = title_bar_width() * title_bar_height() * 4;
	uint32_t* title_bar_data = new uint32_t[title_bar_bytes];
	ASSERT(title_bar_data);
	for (size_t i = 0; i < title_bar_bytes; i++)
		title_bar_data[i] = 0xFFFFFFFF;

	const auto text_area = title_text_area();

	for (size_t i = 0; i < m_title.size() && (i + 1) * font.width() < static_cast<uint32_t>(text_area.width); i++)
	{
		const auto* glyph = font.glyph(m_title[i]);
		if (glyph == nullptr)
			continue;

		const int32_t y_off = (font.height() < (uint32_t)title_bar_height()) ? (title_bar_height() - font.height()) / 2 : 0;
		const int32_t x_off = y_off + i * font.width();
		for (int32_t y = 0; (uint32_t)y < font.height(); y++)
		{
			if (y + y_off >= title_bar_height())
				break;
			for (int32_t x = 0; (uint32_t)x < font.width(); x++)
			{
				if (x + x_off >= text_area.width)
					break;
				const uint8_t bitmask = 1 << (font.width() - x - 1);
				if (glyph[y * font.pitch()] & bitmask)
					title_bar_data[(y_off + y) * title_bar_width() + (x_off + x)] = 0xFF000000;
			}
		}
	}

	if (m_title_bar_data)
		delete[] m_title_bar_data;
	m_title_bar_data = title_bar_data;
}
