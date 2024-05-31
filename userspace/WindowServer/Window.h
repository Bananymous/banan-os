#pragma once

#include "Utils.h"

#include <BAN/RefPtr.h>

class Window : public BAN::RefCounted<Window>
{
public:
	Window(int fd)
		: m_client_fd(fd)
	{ }

	void set_position(Position position)
	{
		m_client_area.x = position.x;
		m_client_area.y = position.y;
	}

	void set_size(Position size, uint32_t* fb_addr)
	{
		m_client_area.width = size.x;
		m_client_area.height = size.y;
		m_fb_addr = fb_addr;
	}

	bool is_deleted() const { return m_deleted; }
	void mark_deleted() { m_deleted = true; }

	int client_fd() const { return m_client_fd; }

	int32_t client_x() const { return m_client_area.x; }
	int32_t client_y() const { return m_client_area.y; }
	int32_t client_width() const { return m_client_area.width; }
	int32_t client_height() const { return m_client_area.height; }
	Rectangle client_size() const { return { 0, 0, client_width(), client_height() }; }
	Rectangle client_area() const { return m_client_area; }

	int32_t title_bar_x() const { return client_x(); }
	int32_t title_bar_y() const { return client_y() - title_bar_height(); }
	int32_t title_bar_width() const { return client_width(); }
	int32_t title_bar_height() const { return m_title_bar_height; }
	Rectangle title_bar_size() const { return { 0, 0, title_bar_width(), title_bar_height() }; }
	Rectangle title_bar_area() const { return { title_bar_x(), title_bar_y(), title_bar_width(), title_bar_height() }; }

	int32_t full_x() const { return title_bar_x(); }
	int32_t full_y() const { return title_bar_y(); }
	int32_t full_width() const { return client_width(); }
	int32_t full_height() const { return client_height() + title_bar_height(); }
	Rectangle full_size() const { return { 0, 0, full_width(), full_height() }; }
	Rectangle full_area() const { return { full_x(), full_y(), full_width(), full_height() }; }

	const uint32_t* framebuffer() const { return m_fb_addr; }

	uint32_t title_bar_pixel(int32_t abs_x, int32_t abs_y, Position cursor) const
	{
		ASSERT(title_bar_area().contains({ abs_x, abs_y }));

		Rectangle close_button = {
			title_bar_x() + title_bar_width() - title_bar_height() + 1,
			title_bar_y() + 1,
			title_bar_height() - 2,
			title_bar_height() - 2
		};

		if (close_button.contains({ abs_x, abs_y }))
			return close_button.contains(cursor) ? 0xFF0000 : 0xA00000;

		return 0xFFFFFF;
	}

private:
	static constexpr int32_t m_title_bar_height { 20 };

	const int	m_client_fd		{ -1 };
	uint32_t*	m_fb_addr		{ nullptr };
	Rectangle	m_client_area	{ 0, 0, 0, 0 };
	bool		m_deleted		{ false };
};
