#include "Cursor.h"
#include "WindowServer.h"

#include <LibGUI/Window.h>
#include <LibInput/KeyboardLayout.h>

#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>

void WindowServer::add_window(int fd, BAN::RefPtr<Window> window)
{
	MUST(m_windows_ordered.insert(0, window));
	MUST(m_windows.insert(fd, window));
	set_focused_window(window);
}

void WindowServer::for_each_window(const BAN::Function<BAN::Iteration(int, Window&)>& callback)
{
	BAN::Vector<int> deleted_windows;
	for (auto it = m_windows.begin(); it != m_windows.end(); it++)
	{
		auto ret = callback(it->key, *it->value);
		if (it->value->is_deleted())
			MUST(deleted_windows.push_back(it->key));
		if (ret == BAN::Iteration::Break)
			break;
		ASSERT(ret == BAN::Iteration::Continue);
	}
	for (int fd : deleted_windows)
	{
		auto window = m_windows[fd];
		m_windows.remove(fd);
		for (size_t i = 0; i < m_windows_ordered.size(); i++)
		{
			if (m_windows_ordered[i] == window)
			{
				m_windows_ordered.remove(i);
				break;
			}
		}
	}
}

void WindowServer::on_key_event(LibInput::KeyEvent event)
{
	// Mod key is not passed to clients
	if (event.key == LibInput::Key::Super)
	{
		m_is_mod_key_held = event.pressed();
		return;
	}

	// Quick hack to stop the window server
	if (event.pressed() && event.key == LibInput::Key::Escape)
		exit(0);

	if (m_focused_window)
	{
		LibGUI::EventPacket packet;
		packet.type = LibGUI::EventPacket::Type::KeyEvent;
		packet.key_event = event;
		send(m_focused_window->client_fd(), &packet, sizeof(packet), 0);
	}
}

void WindowServer::on_mouse_button(LibInput::MouseButtonEvent event)
{
	BAN::RefPtr<Window> target_window;
	for (size_t i = m_windows_ordered.size(); i > 0; i--)
	{
		if (m_windows_ordered[i - 1]->area().contains(m_cursor))
		{
			target_window = m_windows_ordered[i - 1];
			break;
		}
	}

	// Ignore mouse button events which are not on top of a window
	if (!target_window)
		return;

	set_focused_window(target_window);

	// Handle window moving when mod key is held
	if (m_is_mod_key_held && event.pressed && event.button == LibInput::MouseButton::Left && !m_is_moving_window)
		m_is_moving_window = true;
	else if (m_is_moving_window && !event.pressed)
		m_is_moving_window = false;
	else
	{
		// NOTE: we always have target window if code reaches here
		LibGUI::EventPacket packet;
		packet.type = LibGUI::EventPacket::Type::MouseButtonEvent;
		packet.mouse_button_event.button = event.button;
		packet.mouse_button_event.pressed = event.pressed;
		packet.mouse_button_event.x = m_cursor.x - m_focused_window->x();
		packet.mouse_button_event.y = m_cursor.y - m_focused_window->y();
		send(m_focused_window->client_fd(), &packet, sizeof(packet), 0);
	}
}

void WindowServer::on_mouse_move(LibInput::MouseMoveEvent event)
{
	Rectangle old_cursor { m_cursor.x, m_cursor.y, s_cursor_width, s_cursor_height };

	const int32_t new_x = BAN::Math::clamp(m_cursor.x + event.rel_x, 0, m_framebuffer.width);
	const int32_t new_y = BAN::Math::clamp(m_cursor.y - event.rel_y, 0, m_framebuffer.height);

	event.rel_x = new_x - m_cursor.x;
	event.rel_y = new_y - m_cursor.y;
	if (event.rel_x == 0 && event.rel_y == 0)
		return;

	m_cursor.x = new_x;
	m_cursor.y = new_y;

	Rectangle new_cursor { m_cursor.x, m_cursor.y, s_cursor_width, s_cursor_height };
	invalidate(old_cursor.get_bounding_box(old_cursor));
	invalidate(new_cursor.get_bounding_box(old_cursor));

	if (m_is_moving_window)
	{
		auto old_window = m_focused_window->area();
		m_focused_window->set_position({
			m_focused_window->x() + event.rel_x,
			m_focused_window->y() + event.rel_y,
		});
		invalidate(old_window);
		invalidate(m_focused_window->area());
		return;
	}

	if (m_focused_window)
	{
		LibGUI::EventPacket packet;
		packet.type = LibGUI::EventPacket::Type::MouseMoveEvent;
		packet.mouse_move_event.x = m_cursor.x - m_focused_window->x();
		packet.mouse_move_event.y = m_cursor.y - m_focused_window->y();
		send(m_focused_window->client_fd(), &packet, sizeof(packet), 0);
	}
}

void WindowServer::on_mouse_scroll(LibInput::MouseScrollEvent event)
{
	if (m_focused_window)
	{
		LibGUI::EventPacket packet;
		packet.type = LibGUI::EventPacket::Type::MouseScrollEvent;
		packet.mouse_scroll_event = event;
		send(m_focused_window->client_fd(), &packet, sizeof(packet), 0);
	}
}

void WindowServer::set_focused_window(BAN::RefPtr<Window> window)
{
	if (m_focused_window == window)
		return;

	for (size_t i = m_windows_ordered.size(); i > 0; i--)
	{
		if (m_windows_ordered[i - 1] == window)
		{
			m_focused_window = window;
			m_windows_ordered.remove(i - 1);
			MUST(m_windows_ordered.push_back(window));
			invalidate(window->area());
			break;
		}
	}
}

void WindowServer::invalidate(Rectangle area)
{
	auto fb_overlap = area.get_overlap(m_framebuffer.area());
	if (!fb_overlap.has_value())
		return;
	area = fb_overlap.release_value();

	for (int32_t y = area.y; y < area.y + area.height; y++)
		memset(&m_framebuffer.mmap[y * m_framebuffer.width + area.x], 0, area.width * 4);

	for (auto& pwindow : m_windows_ordered)
	{
		auto& window = *pwindow;

		auto overlap = window.area().get_overlap(area);
		if (!overlap.has_value())
			continue;

		const int32_t src_x = overlap->x - window.x();
		const int32_t src_y = overlap->y - window.y();
		for (int32_t y_off = 0; y_off < overlap->height; y_off++)
			memcpy(
				&m_framebuffer.mmap[(overlap->y + y_off) * m_framebuffer.width + overlap->x],
				&window.framebuffer()[(src_y + y_off) * window.width() + src_x],
				overlap->width * 4
			);
	}

	Rectangle cursor { m_cursor.x, m_cursor.y, s_cursor_width, s_cursor_height };
	auto overlap = cursor.get_overlap(area);
	if (overlap.has_value())
	{
		for (int32_t dy = overlap->y - cursor.y; dy < overlap->height; dy++)
		{
			for (int32_t dx = overlap->x - cursor.x; dx < overlap->width; dx++)
			{
				const uint32_t offset = (dy * s_cursor_width + dx) * 4;
				uint32_t r = (((s_cursor_data[offset + 0] - 33) << 2) | ((s_cursor_data[offset + 1] - 33) >> 4));
				uint32_t g = ((((s_cursor_data[offset + 1] - 33) & 0xF) << 4) | ((s_cursor_data[offset + 2] - 33) >> 2));
				uint32_t b = ((((s_cursor_data[offset + 2] - 33) & 0x3) << 6) | ((s_cursor_data[offset + 3] - 33)));
				uint32_t color = (r << 16) | (g << 8) | b;
				if (color != 0xFF00FF)
					m_framebuffer.mmap[(overlap->y + dy) * m_framebuffer.width + (overlap->x + dx)] = color;
			}
		}
	}

	uintptr_t mmap_start = reinterpret_cast<uintptr_t>(m_framebuffer.mmap) + area.y * m_framebuffer.width * 4;
	uintptr_t mmap_end = mmap_start + (area.height + 1) * m_framebuffer.width * 4;
	mmap_start &= ~(uintptr_t)0xFFF;
	msync(reinterpret_cast<void*>(mmap_start), mmap_end - mmap_start, MS_SYNC);
}
