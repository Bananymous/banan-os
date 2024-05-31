#pragma once

#include "Framebuffer.h"
#include "Window.h"

#include <BAN/Function.h>
#include <BAN/Iteration.h>
#include <BAN/Vector.h>
#include <BAN/HashMap.h>

#include <LibInput/KeyEvent.h>
#include <LibInput/MouseEvent.h>

class WindowServer
{
public:
	WindowServer(Framebuffer& framebuffer)
		: m_framebuffer(framebuffer)
		, m_cursor({ framebuffer.width / 2, framebuffer.height / 2 })
	{
		invalidate(m_framebuffer.area());
	}

	void add_window(int fd, BAN::RefPtr<Window> window);
	void for_each_window(const BAN::Function<BAN::Iteration(int, Window&)>& callback);

	void on_key_event(LibInput::KeyEvent event);
	void on_mouse_button(LibInput::MouseButtonEvent event);
	void on_mouse_move(LibInput::MouseMoveEvent event);
	void on_mouse_scroll(LibInput::MouseScrollEvent event);

	void set_focused_window(BAN::RefPtr<Window> window);
	void invalidate(Rectangle area);

	Rectangle cursor_area() const;

private:
	Framebuffer& m_framebuffer;
	BAN::Vector<BAN::RefPtr<Window>> m_windows_ordered;
	BAN::HashMap<int, BAN::RefPtr<Window>> m_windows;

	bool m_is_mod_key_held { false };
	bool m_is_moving_window { false };
	BAN::RefPtr<Window> m_focused_window;
	Position m_cursor;
};
