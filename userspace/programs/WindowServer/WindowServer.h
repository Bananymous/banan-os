#pragma once

#include "Framebuffer.h"
#include "Window.h"

#include <BAN/Array.h>
#include <BAN/Function.h>
#include <BAN/Iteration.h>
#include <BAN/Vector.h>
#include <BAN/HashMap.h>

#include <LibFont/Font.h>
#include <LibGUI/Window.h>
#include <LibImage/Image.h>
#include <LibInput/KeyEvent.h>
#include <LibInput/MouseEvent.h>

#include <sys/select.h>

class WindowServer
{
public:
	struct ClientData
	{
		size_t packet_buffer_nread = 0;
		BAN::Vector<uint8_t> packet_buffer;
	};

public:
	WindowServer(Framebuffer& framebuffer, int32_t corner_radius);

	BAN::ErrorOr<void> set_background_image(BAN::UniqPtr<LibImage::Image>);

	void on_window_create(int fd, const LibGUI::WindowPacket::WindowCreate&);
	void on_window_invalidate(int fd, const LibGUI::WindowPacket::WindowInvalidate&);
	void on_window_set_position(int fd, const LibGUI::WindowPacket::WindowSetPosition&);
	void on_window_set_attributes(int fd, const LibGUI::WindowPacket::WindowSetAttributes&);
	void on_window_set_mouse_capture(int fd, const LibGUI::WindowPacket::WindowSetMouseCapture&);
	void on_window_set_size(int fd, const LibGUI::WindowPacket::WindowSetSize&);
	void on_window_set_min_size(int fd, const LibGUI::WindowPacket::WindowSetMinSize&);
	void on_window_set_max_size(int fd, const LibGUI::WindowPacket::WindowSetMaxSize&);
	void on_window_set_fullscreen(int fd, const LibGUI::WindowPacket::WindowSetFullscreen&);
	void on_window_set_title(int fd, const LibGUI::WindowPacket::WindowSetTitle&);

	void on_key_event(LibInput::KeyEvent event);
	void on_mouse_button(LibInput::MouseButtonEvent event);
	void on_mouse_move(LibInput::MouseMoveEvent event);
	void on_mouse_scroll(LibInput::MouseScrollEvent event);

	void set_focused_window(BAN::RefPtr<Window> window);
	void invalidate(Rectangle area);
	void sync();

	Rectangle cursor_area() const;
	Rectangle resize_area(Position cursor) const;

	void add_client_fd(int fd);
	void remove_client_fd(int fd);
	int get_client_fds(fd_set& fds) const;
	void for_each_client_fd(const BAN::Function<BAN::Iteration(int, ClientData&)>& callback);

	bool is_stopped() const { return m_is_stopped; }

private:
	void mark_pending_sync(Rectangle area);

	bool resize_window(BAN::RefPtr<Window> window, uint32_t width, uint32_t height) const;

private:
	struct RangeList
	{
		size_t range_count { 0 };
		BAN::Array<Range, 32> ranges;
		void add_range(const Range& range);
	};

	enum class State
	{
		Normal,
		Fullscreen,
		Moving,
		Resizing,
	};

private:
	Framebuffer& m_framebuffer;
	BAN::Vector<BAN::RefPtr<Window>> m_client_windows;

	BAN::HashMap<int, ClientData> m_client_data;

	const int32_t m_corner_radius;

	BAN::Vector<RangeList> m_pending_syncs;

	BAN::UniqPtr<LibImage::Image> m_background_image;

	State m_state { State::Normal };
	bool m_is_mod_key_held { false };
	BAN::RefPtr<Window> m_focused_window;
	BAN::Array<BAN::RefPtr<Window>, 5> m_mouse_button_windows;
	Position m_cursor;

	Rectangle m_non_full_screen_rect;

	uint8_t m_resize_quadrant { 0 };
	Position m_resize_start;

	bool m_is_mouse_captured { false };

	bool m_deleted_window { false };
	bool m_is_stopped { false };
	bool m_is_bouncing_window = false;

	LibFont::Font m_font;
};
