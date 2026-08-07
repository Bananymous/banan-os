#include "Window.h"

#include <BAN/Debug.h>

#include <LibGUI/Window.h>

#include <sys/shm.h>
#include <sys/socket.h>
#include <unistd.h>

Window::~Window()
{
	shmdt(m_fb_addr);

	LibGUI::EventPacket::DestroyWindowEvent packet;

	BAN::Vector<uint8_t> buffer;
	if (!buffer.resize(packet.serialized_size()).is_error())
	{
		packet.serialize(buffer.span());

		size_t total_sent = 0;
		while (total_sent < buffer.size())
		{
			const ssize_t nsend = send(m_client_fd, buffer.data() + total_sent, buffer.size() - total_sent, 0);
			if (nsend <= 0)
				break;
			total_sent += nsend;
		}
	}
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

	const int shmid = shmget(IPC_PRIVATE, fb_bytes, 0666);
	if (shmid == -1)
		return BAN::Error::from_errno(errno);

	uint32_t* fb_addr = static_cast<uint32_t*>(shmat(shmid, nullptr, 0));
	shmctl(shmid, IPC_RMID, nullptr);

	if (fb_addr == SHM_FAILED)
		return BAN::Error::from_errno(errno);
	memset(fb_addr, 0xFF, fb_bytes);

	{
		const auto old_area = m_client_area;

		m_client_area.max_x = m_client_area.min_x + width;
		m_client_area.max_y = m_client_area.min_y + height;
		auto title_bar_ret = prepare_title_bar();
		m_client_area = old_area;

		if (title_bar_ret.is_error())
		{
			shmdt(fb_addr);
			return title_bar_ret.release_error();
		}
	}

	if (m_fb_addr != nullptr)
		shmdt(m_fb_addr);

	m_fb_addr = fb_addr;
	m_shmid   = shmid;

	m_client_area.max_x = m_client_area.min_x + width;
	m_client_area.max_y = m_client_area.min_y + height;

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

	for (size_t i = 0; i < m_title.size() && (i + 1) * font_w < static_cast<uint32_t>(text_area.width()); i++)
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
				if (x + x_off >= text_area.width())
					break;
				const uint8_t bitmask = 1 << (font_w - x - 1);
				if (glyph[y * font_p] & bitmask)
					m_title_bar_data[(y_off + y) * title_bar_width() + (x_off + x)] = 0xFF000000;
			}
		}
	}

	return {};
}
