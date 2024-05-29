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
		m_area.x = position.x;
		m_area.y = position.y;
	}

	void set_size(Position size, uint32_t* fb_addr)
	{
		m_area.width = size.x;
		m_area.height = size.y;
		m_fb_addr = fb_addr;
	}

	bool is_deleted() const { return m_deleted; }
	void mark_deleted() { m_deleted = true; }

	int client_fd() const { return m_client_fd; }

	int32_t x() const { return m_area.x; }
	int32_t y() const { return m_area.y; }
	uint32_t width() const { return m_area.width; }
	uint32_t height() const { return m_area.height; }
	Rectangle size() const { return { 0, 0, m_area.width, m_area.height }; }
	const Rectangle& area() const { return m_area; }
	const uint32_t* framebuffer() const { return m_fb_addr; }

private:
	const int	m_client_fd	{ -1 };
	uint32_t*	m_fb_addr	{ nullptr };
	Rectangle	m_area		{ 0, 0, 0, 0 };
	bool		m_deleted	{ false };
};
