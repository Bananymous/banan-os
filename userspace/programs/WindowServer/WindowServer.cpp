#include "Cursor.h"
#include "WindowServer.h"

#include <BAN/Debug.h>
#include <BAN/ScopeGuard.h>

#include <LibGUI/Window.h>
#include <LibInput/KeyboardLayout.h>

#include <stdlib.h>
#include <sys/banan-os.h>
#include <sys/mman.h>
#include <sys/socket.h>

#include <unistd.h>

WindowServer::WindowServer(Framebuffer& framebuffer, int32_t corner_radius)
	: m_framebuffer(framebuffer)
	, m_corner_radius(corner_radius)
	, m_cursor({ framebuffer.width / 2, framebuffer.height / 2 })
	, m_font(MUST(LibFont::Font::load("/usr/share/fonts/lat0-16.psfu"_sv)))
{
	BAN::Vector<LibImage::Image::Color> bitmap;
	MUST(bitmap.resize(m_framebuffer.width * m_framebuffer.height, { 0x10, 0x10, 0x10, 0xFF }));
	m_background_image = MUST(BAN::UniqPtr<LibImage::Image>::create(m_framebuffer.width, m_framebuffer.height, BAN::move(bitmap)));

	MUST(m_pending_syncs.resize(m_framebuffer.height));

	invalidate(m_framebuffer.area());
}

BAN::ErrorOr<void> WindowServer::set_background_image(BAN::UniqPtr<LibImage::Image> image)
{
	if (image->width() != (uint64_t)m_framebuffer.width || image->height() != (uint64_t)m_framebuffer.height)
		image = TRY(image->resize(m_framebuffer.width, m_framebuffer.height));
	m_background_image = BAN::move(image);
	invalidate(m_framebuffer.area());
	return {};
}

void WindowServer::on_window_create(int fd, const LibGUI::WindowPacket::WindowCreate& packet)
{
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		dwarnln("client with window tried to create another one");
		return;
	}

	const uint32_t width = packet.width ? packet.width : m_framebuffer.width;
	const uint32_t height = packet.height ? packet.height : m_framebuffer.height;

	auto window_or_error = BAN::RefPtr<Window>::create(fd, m_font);
	if (window_or_error.is_error())
	{
		dwarnln("could not create window for client: {}", window_or_error.error());
		return;
	}
	auto window = window_or_error.release_value();

	if (auto ret = m_client_windows.push_back(window); ret.is_error())
	{
		dwarnln("could not create window for client: {}", ret.error());
		return;
	}
	BAN::ScopeGuard window_popper([&] { m_client_windows.pop_back(); });

	if (auto ret = window->initialize(packet.title, width, height); ret.is_error())
	{
		dwarnln("could not create window for client: {}", ret.error());
		return;
	}

	window->set_attributes(packet.attributes);
	window->set_position({
		static_cast<int32_t>((m_framebuffer.width - window->client_width()) / 2),
		static_cast<int32_t>((m_framebuffer.height - window->client_height()) / 2),
	});

	LibGUI::EventPacket::ResizeWindowEvent response;
	response.width = window->client_width();
	response.height = window->client_height();
	response.smo_key = window->smo_key();
	if (auto ret = response.send_serialized(fd); ret.is_error())
	{
		dwarnln("could not respond to window create request: {}", ret.error());
		return;
	}

	window_popper.disable();

	if (packet.attributes.focusable)
		set_focused_window(window);
	else if (m_client_windows.size() > 1)
		BAN::swap(m_client_windows[m_client_windows.size() - 1], m_client_windows[m_client_windows.size() - 2]);
}

void WindowServer::on_window_invalidate(int fd, const LibGUI::WindowPacket::WindowInvalidate& packet)
{
	if (packet.width == 0 || packet.height == 0)
		return;

	if (m_state == State::Fullscreen)
	{
		ASSERT(m_focused_window);
		if (m_focused_window->client_fd() != fd)
			return;
	}

	BAN::RefPtr<Window> target_window;
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		target_window = window;
		break;
	}

	if (!target_window)
	{
		dwarnln("client tried to invalidate window while not owning a window");
		return;
	}

	invalidate({
		target_window->client_x() + static_cast<int32_t>(packet.x),
		target_window->client_y() + static_cast<int32_t>(packet.y),
		BAN::Math::min<int32_t>(packet.width,  target_window->client_width()),
		BAN::Math::min<int32_t>(packet.height, target_window->client_height())
	});
}

void WindowServer::on_window_set_position(int fd, const LibGUI::WindowPacket::WindowSetPosition& packet)
{
	if (m_state == State::Fullscreen)
	{
		ASSERT(m_focused_window);
		if (m_focused_window->client_fd() != fd)
			return;
	}

	BAN::RefPtr<Window> target_window;
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		target_window = window;
		break;
	}

	if (!target_window)
	{
		dwarnln("client tried to set window position while not owning a window");
		return;
	}

	const auto old_client_area = target_window->full_area();
	target_window->set_position({
		.x = packet.x,
		.y = packet.y,
	});
	const auto new_client_area = target_window->full_area();
	invalidate(new_client_area.get_bounding_box(old_client_area));
}

void WindowServer::on_window_set_attributes(int fd, const LibGUI::WindowPacket::WindowSetAttributes& packet)
{
	BAN::RefPtr<Window> target_window;
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		target_window = window;
		break;
	}

	if (!target_window)
	{
		dwarnln("client tried to set window attributes while not owning a window");
		return;
	}

	const auto old_client_area = target_window->full_area();
	target_window->set_attributes(packet.attributes);
	const auto new_client_area = target_window->full_area();
	invalidate(new_client_area.get_bounding_box(old_client_area));

	if (!packet.attributes.focusable && m_focused_window == target_window)
	{
		m_focused_window = nullptr;
		for (size_t i = m_client_windows.size(); i > 0; i--)
		{
			if (auto& window = m_client_windows[i - 1]; window->get_attributes().focusable)
			{
				set_focused_window(window);
				break;
			}
		}
	}
}

void WindowServer::on_window_set_mouse_capture(int fd, const LibGUI::WindowPacket::WindowSetMouseCapture& packet)
{
	if (m_is_mouse_captured && packet.captured)
	{
		ASSERT(m_focused_window);
		if (fd != m_focused_window->client_fd())
			dwarnln("client tried to set mouse capture while other window has it already captured");
		return;
	}

	BAN::RefPtr<Window> target_window;
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		target_window = window;
		break;
	}

	if (!target_window)
	{
		dwarnln("client tried to set mouse capture while not owning a window");
		return;
	}

	if (packet.captured == m_is_mouse_captured)
		return;

	set_focused_window(target_window);
	m_is_mouse_captured = packet.captured;
	invalidate(cursor_area());
}

void WindowServer::on_window_set_size(int fd, const LibGUI::WindowPacket::WindowSetSize& packet)
{
	BAN::RefPtr<Window> target_window;
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		target_window = window;
		break;
	}

	if (!target_window)
	{
		dwarnln("client tried to set window size while not owning a window");
		return;
	}

	const auto old_area = target_window->full_area();

	const uint32_t width = packet.width ? packet.width : m_framebuffer.width;
	const uint32_t height = packet.height ? packet.height : m_framebuffer.height;

	if (!resize_window(target_window, width, height))
		return;

	invalidate(target_window->full_area().get_bounding_box(old_area));
}

void WindowServer::on_window_set_min_size(int fd, const LibGUI::WindowPacket::WindowSetMinSize& packet)
{
	BAN::RefPtr<Window> target_window;
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		target_window = window;
		break;
	}

	if (!target_window)
	{
		dwarnln("client tried to set window min size while not owning a window");
		return;
	}

	target_window->set_min_size({ 0, 0, static_cast<int32_t>(packet.width), static_cast<int32_t>(packet.height) });
}

void WindowServer::on_window_set_fullscreen(int fd, const LibGUI::WindowPacket::WindowSetFullscreen& packet)
{
	if (m_state == State::Fullscreen)
	{
		if (m_focused_window->client_fd() != fd)
		{
			dwarnln("client tried to set fullscreen state while another window is fullscreen");
			return;
		}
		if (packet.fullscreen)
			return;
		if (m_focused_window->get_attributes().resizable)
		{
			if (!resize_window(m_focused_window, m_non_full_screen_rect.width, m_non_full_screen_rect.height))
				return;
			m_focused_window->set_position({ m_non_full_screen_rect.x, m_non_full_screen_rect.y });
		}
		m_state = State::Normal;
		invalidate(m_framebuffer.area());
		return;
	}

	if (!packet.fullscreen)
		return;

	BAN::RefPtr<Window> target_window;
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		target_window = window;
		break;
	}

	if (!target_window)
	{
		dwarnln("client tried to set window fullscreen while not owning a window");
		return;
	}

	if (target_window->get_attributes().resizable)
	{
		const auto old_area = target_window->client_area();
		if (!resize_window(target_window, m_framebuffer.width, m_framebuffer.height))
			return;
		target_window->set_position({ 0, 0 });
		m_non_full_screen_rect = old_area;
	}

	m_state = State::Fullscreen;
	set_focused_window(target_window);
	invalidate(m_framebuffer.area());
}

void WindowServer::on_window_set_title(int fd, const LibGUI::WindowPacket::WindowSetTitle& packet)
{
	BAN::RefPtr<Window> target_window;
	for (auto& window : m_client_windows)
	{
		if (window->client_fd() != fd)
			continue;
		target_window = window;
		break;
	}

	if (!target_window)
	{
		dwarnln("client tried to set window title while not owning a window");
		return;
	}

	if (auto ret = target_window->set_title(packet.title); ret.is_error())
	{
		dwarnln("failed to set window title: {}", ret.error());
		return;
	}

	invalidate(target_window->title_bar_area());
}

void WindowServer::on_key_event(LibInput::KeyEvent event)
{
	// Mod key is not passed to clients
	if (event.key == LibInput::Key::LeftAlt)
	{
		m_is_mod_key_held = event.pressed();
		return;
	}

	// Stop WindowServer with mod+shift+E
	if (m_is_mod_key_held && event.pressed() && event.shift() && event.key == LibInput::Key::E)
	{
		m_is_stopped = true;
		return;
	}

	// Start terminal with mod+Enter
	if (m_is_mod_key_held && event.pressed() && event.key == LibInput::Key::Enter)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			execl("/usr/bin/Terminal", "Terminal", nullptr);
			exit(1);
		}
		if (pid == -1)
			perror("fork");
		return;
	}

	// Toggle window bounce with F2
	if (event.pressed() && event.key == LibInput::Key::F2)
	{
		m_is_bouncing_window = !m_is_bouncing_window;
		return;
	}

	if (!m_focused_window)
		return;

	// Kill window with mod+Q
	if (m_is_mod_key_held && event.pressed() && event.key == LibInput::Key::Q)
	{
		LibGUI::EventPacket::CloseWindowEvent packet;
		if (auto ret = packet.send_serialized(m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send window close event: {}", ret.error());
		return;
	}

	if (m_is_mod_key_held && event.pressed() && event.key == LibInput::Key::F)
	{
		if (m_state == State::Fullscreen)
		{
			if (m_focused_window->get_attributes().resizable)
			{
				if (!resize_window(m_focused_window, m_non_full_screen_rect.width, m_non_full_screen_rect.height))
					return;
				m_focused_window->set_position({ m_non_full_screen_rect.x, m_non_full_screen_rect.y });
			}
			m_state = State::Normal;
		}
		else
		{
			if (m_focused_window->get_attributes().resizable)
			{
				const auto old_area = m_focused_window->client_area();
				if (!resize_window(m_focused_window, m_framebuffer.width, m_framebuffer.height))
					return;
				m_focused_window->set_position({ 0, 0 });
				m_non_full_screen_rect = old_area;
			}
			m_state = State::Fullscreen;
		}

		invalidate(m_framebuffer.area());
		return;
	}

	LibGUI::EventPacket::KeyEvent packet;
	packet.event = event;
	if (auto ret = packet.send_serialized(m_focused_window->client_fd()); ret.is_error())
		dwarnln("could not send key event: {}", ret.error());
}

void WindowServer::on_mouse_button(LibInput::MouseButtonEvent event)
{
	if (m_is_mouse_captured)
	{
		ASSERT(m_focused_window);

		LibGUI::EventPacket::MouseButtonEvent packet;
		packet.event.button = event.button;
		packet.event.pressed = event.pressed;
		packet.event.x = 0;
		packet.event.y = 0;
		if (auto ret = packet.send_serialized(m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send mouse button event: {}", ret.error());
		return;
	}

	BAN::RefPtr<Window> target_window;
	for (size_t i = m_client_windows.size(); i > 0; i--)
	{
		if (m_client_windows[i - 1]->full_area().contains(m_cursor))
		{
			target_window = m_client_windows[i - 1];
			break;
		}
	}

	switch (m_state)
	{
		case State::Normal:
			if (!target_window)
				break;

			if (target_window->get_attributes().focusable)
				set_focused_window(target_window);

			if (event.button == LibInput::MouseButton::Left && event.pressed)
			{
				const bool can_start_move = m_is_mod_key_held || target_window->title_text_area().contains(m_cursor);
				if (can_start_move && target_window->get_attributes().movable)
				{
					m_state = State::Moving;
					break;
				}
			}

			if (event.button == LibInput::MouseButton::Right && event.pressed)
			{
				const bool can_start_resize = m_is_mod_key_held;
				if (can_start_resize && target_window->get_attributes().resizable)
				{
					m_state = State::Resizing;
					m_resize_start = m_cursor;

					const bool right  = m_cursor.x >= target_window->full_x() + target_window->full_width()  / 2;
					const bool bottom = m_cursor.y >= target_window->full_y() + target_window->full_height() / 2;
					m_resize_quadrant = right + 2 * bottom;

					break;
				}
			}

			if (event.button == LibInput::MouseButton::Left && !event.pressed && target_window->close_button_area().contains(m_cursor))
			{
				LibGUI::EventPacket::CloseWindowEvent packet;
				if (auto ret = packet.send_serialized(m_focused_window->client_fd()); ret.is_error())
					dwarnln("could not send close window event: {}", ret.error());
				break;
			}

			[[fallthrough]];
		case State::Fullscreen:
			if (target_window && target_window->client_area().contains(m_cursor))
			{
				LibGUI::EventPacket::MouseButtonEvent packet;
				packet.event.button = event.button;
				packet.event.pressed = event.pressed;
				packet.event.x = m_cursor.x - m_focused_window->client_x();
				packet.event.y = m_cursor.y - m_focused_window->client_y();
				if (auto ret = packet.send_serialized(m_focused_window->client_fd()); ret.is_error())
				{
					dwarnln("could not send mouse button event: {}", ret.error());
					return;
				}
			}
			break;
		case State::Moving:
			if (event.button == LibInput::MouseButton::Left && !event.pressed)
				m_state = State::Normal;
			break;
		case State::Resizing:
			if (event.button == LibInput::MouseButton::Right && !event.pressed)
			{
				const auto resize_area = this->resize_area(m_cursor);
				m_state = State::Normal;
				invalidate(resize_area.get_bounding_box(m_focused_window->full_area()));

				const auto old_area = m_focused_window->full_area();
				if (auto ret = m_focused_window->resize(resize_area.width, resize_area.height - m_focused_window->title_bar_height()); ret.is_error())
				{
					dwarnln("could not resize client window {}", ret.error());
					return;
				}
				m_focused_window->set_position({ resize_area.x, resize_area.y + m_focused_window->title_bar_height() });

				LibGUI::EventPacket::ResizeWindowEvent event;
				event.width = m_focused_window->client_width();
				event.height = m_focused_window->client_height();
				event.smo_key = m_focused_window->smo_key();
				if (auto ret = event.send_serialized(m_focused_window->client_fd()); ret.is_error())
				{
					dwarnln("could not respond to window resize request: {}", ret.error());
					return;
				}

				invalidate(m_focused_window->full_area().get_bounding_box(old_area));
			}
			break;
	}
}

void WindowServer::on_mouse_move(LibInput::MouseMoveEvent event)
{
	if (m_is_mouse_captured)
	{
		ASSERT(m_focused_window);

		LibGUI::EventPacket::MouseMoveEvent packet;
		packet.event.x =  event.rel_x;
		packet.event.y = -event.rel_y;
		if (auto ret = packet.send_serialized(m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send mouse move event: {}", ret.error());
		return;
	}

	const auto [new_x, new_y] =
		[&]() -> Position
		{
			const int32_t new_x = m_cursor.x + event.rel_x;
			const int32_t new_y = m_cursor.y - event.rel_y;
			return (m_state == State::Fullscreen)
				? Position {
					.x = BAN::Math::clamp(new_x, m_focused_window->client_x(), m_focused_window->client_x() + m_focused_window->client_width()),
					.y = BAN::Math::clamp(new_y, m_focused_window->client_y(), m_focused_window->client_y() + m_focused_window->client_height())
				}
				: Position {
					.x = BAN::Math::clamp(new_x, 0, m_framebuffer.width),
					.y = BAN::Math::clamp(new_y, 0, m_framebuffer.height)
				};
		}();

	event.rel_x = new_x - m_cursor.x;
	event.rel_y = new_y - m_cursor.y;
	if (event.rel_x == 0 && event.rel_y == 0)
		return;

	auto old_cursor = cursor_area();
	m_cursor.x = new_x;
	m_cursor.y = new_y;
	auto new_cursor = cursor_area();

	invalidate(old_cursor);
	invalidate(new_cursor);

	// TODO: Really no need to loop over every window
	for (auto& window : m_client_windows)
	{
		auto title_bar = window->title_bar_area();
		if (title_bar.get_overlap(old_cursor).has_value() || title_bar.get_overlap(new_cursor).has_value())
			invalidate(title_bar);
	}

	if (!m_focused_window)
		return;

	switch (m_state)
	{
		case State::Normal:
		case State::Fullscreen:
		{
			LibGUI::EventPacket::MouseMoveEvent packet;
			packet.event.x = m_cursor.x - m_focused_window->client_x();
			packet.event.y = m_cursor.y - m_focused_window->client_y();
			if (auto ret = packet.send_serialized(m_focused_window->client_fd()); ret.is_error())
			{
				dwarnln("could not send mouse move event: {}", ret.error());
				return;
			}
			break;
		}
		case State::Moving:
		{
			auto old_window = m_focused_window->full_area();
			m_focused_window->set_position({
				m_focused_window->client_x() + event.rel_x,
				m_focused_window->client_y() + event.rel_y,
			});
			auto new_window = m_focused_window->full_area();
			invalidate(old_window);
			invalidate(new_window);
			break;
		}
		case State::Resizing:
		{
			const auto old_resize_area = resize_area({ old_cursor.x, old_cursor.y });
			const auto new_resize_area = resize_area({ new_cursor.x, new_cursor.y });
			invalidate(old_resize_area.get_bounding_box(new_resize_area));
			break;
		}
	}
}

void WindowServer::on_mouse_scroll(LibInput::MouseScrollEvent event)
{
	if (m_focused_window)
	{
		LibGUI::EventPacket::MouseScrollEvent packet;
		packet.event.scroll = event.scroll;
		if (auto ret = packet.send_serialized(m_focused_window->client_fd()); ret.is_error())
		{
			dwarnln("could not send mouse scroll event: {}", ret.error());
			return;
		}
	}
}

void WindowServer::set_focused_window(BAN::RefPtr<Window> window)
{
	ASSERT(window->get_attributes().focusable);
	if (m_focused_window == window)
		return;

	if (m_is_mouse_captured)
	{
		m_is_mouse_captured = false;
		invalidate(cursor_area());
	}

	for (size_t i = m_client_windows.size(); i > 0; i--)
	{
		if (m_client_windows[i - 1] == window)
		{
			m_focused_window = window;
			m_client_windows.remove(i - 1);
			MUST(m_client_windows.push_back(window));
			invalidate(window->full_area());
			break;
		}
	}
}

static uint32_t alpha_blend(uint32_t color_a, uint32_t color_b)
{
	const uint32_t a_a =   color_a >> 24;
	const uint32_t a_b = ((color_b >> 24) * (256 - a_a)) >> 8;
	const uint32_t a = a_a + a_b;

	const uint32_t rb1 = (a_a * (color_a & 0xFF00FF)) >> 8;
	const uint32_t rb2 = (a_b * (color_b & 0xFF00FF)) >> 8;

	const uint32_t g1  = (a_a * (color_a & 0x00FF00)) >> 8;
	const uint32_t g2  = (a_b * (color_b & 0x00FF00)) >> 8;

	return (a << 24) | ((rb1 | rb2) & 0xFF00FF) | ((g1 | g2) & 0x00FF00);
}

void WindowServer::invalidate(Rectangle area)
{
	ASSERT(m_background_image->width() == (uint64_t)m_framebuffer.width);
	ASSERT(m_background_image->height() == (uint64_t)m_framebuffer.height);

	const auto get_cursor_pixel =
		[](int32_t rel_x, int32_t rel_y) -> BAN::Optional<uint32_t>
		{
			const uint32_t offset = (rel_y * s_cursor_width + rel_x) * 4;
			uint32_t r = (((s_cursor_data[offset + 0] - 33) << 2) | ((s_cursor_data[offset + 1] - 33) >> 4));
			uint32_t g = ((((s_cursor_data[offset + 1] - 33) & 0xF) << 4) | ((s_cursor_data[offset + 2] - 33) >> 2));
			uint32_t b = ((((s_cursor_data[offset + 2] - 33) & 0x3) << 6) | ((s_cursor_data[offset + 3] - 33)));
			uint32_t color = (r << 16) | (g << 8) | b;
			if (color == 0xFF00FF)
				return {};
			return color;
		};

	if (m_state == State::Fullscreen)
	{
		ASSERT(m_focused_window);
		area.x -= m_focused_window->client_x();
		area.y -= m_focused_window->client_y();

		const Rectangle client_area {
			.x = 0,
			.y = 0,
			.width  = m_focused_window->client_width(),
			.height = m_focused_window->client_height(),
		};

		auto focused_overlap = area.get_overlap(client_area);
		if (!focused_overlap.has_value())
			return;
		area = focused_overlap.release_value();

		const bool should_alpha_blend = m_focused_window->get_attributes().alpha_channel;

		if (client_area == m_framebuffer.area())
		{
			if (!should_alpha_blend)
			{
				for (int32_t y = area.y; y < area.y + area.height; y++)
					for (int32_t x = area.x; x < area.x + area.width; x++)
						m_framebuffer.mmap[y * m_framebuffer.width + x] = m_focused_window->framebuffer()[y * m_focused_window->client_width() + x];
			}
			else
			{
				for (int32_t y = area.y; y < area.y + area.height; y++)
				{
					for (int32_t x = area.x; x < area.x + area.width; x++)
					{
						const uint32_t src_pixel = m_focused_window->framebuffer()[y * m_focused_window->client_width() + x];
						const uint32_t bg_pixel = m_background_image->get_color(x, y).as_argb();
						m_framebuffer.mmap[y * m_framebuffer.width + x] = alpha_blend(src_pixel, bg_pixel);
					}
				}
			}
			mark_pending_sync(area);
		}
		else
		{
			auto opt_dst_area = Rectangle {
				.x = area.x * m_framebuffer.width  / m_focused_window->client_width(),
				.y = area.y * m_framebuffer.height / m_focused_window->client_height(),
				.width  = BAN::Math::div_round_up(area.width  * m_framebuffer.width,  m_focused_window->client_width()),
				.height = BAN::Math::div_round_up(area.height * m_framebuffer.height, m_focused_window->client_height())
			}.get_overlap(m_framebuffer.area());
			if (!opt_dst_area.has_value())
				return;
			const auto dst_area = opt_dst_area.release_value();

			for (int32_t dst_y = dst_area.y; dst_y < dst_area.y + dst_area.height; dst_y++)
			{
				for (int32_t dst_x = dst_area.x; dst_x < dst_area.x + dst_area.width; dst_x++)
				{
					const int32_t src_x = BAN::Math::clamp<int32_t>(dst_x * m_focused_window->client_width()  / m_framebuffer.width,  0, m_focused_window->client_width());
					const int32_t src_y = BAN::Math::clamp<int32_t>(dst_y * m_focused_window->client_height() / m_framebuffer.height, 0, m_focused_window->client_height());

					const uint32_t src_pixel = m_focused_window->framebuffer()[src_y * m_focused_window->client_width() + src_x];
					const uint32_t bg_pixel = m_background_image->get_color(dst_x, dst_y).as_argb();

					uint32_t& dst_pixel = m_framebuffer.mmap[dst_y * m_framebuffer.width + dst_x];
					dst_pixel = should_alpha_blend ? alpha_blend(src_pixel, bg_pixel) : src_pixel;
				}
			}

			mark_pending_sync(dst_area);
		}

		if (!m_is_mouse_captured)
		{
			const Rectangle cursor_area {
				.x = m_cursor.x - m_focused_window->client_x(),
				.y = m_cursor.y - m_focused_window->client_y(),
				.width  = s_cursor_width,
				.height = s_cursor_height,
			};

			if (!area.get_overlap(cursor_area).has_value())
				return;

			const int32_t cursor_tl_dst_x = cursor_area.x * m_framebuffer.width  / m_focused_window->client_width();
			const int32_t cursor_tl_dst_y = cursor_area.y * m_framebuffer.height / m_focused_window->client_height();

			for (int32_t rel_y = 0; rel_y < s_cursor_height; rel_y++)
			{
				for (int32_t rel_x = 0; rel_x < s_cursor_width; rel_x++)
				{
					const auto pixel = get_cursor_pixel(rel_x, rel_y);
					if (!pixel.has_value())
						continue;

					const int32_t dst_x = cursor_tl_dst_x + rel_x;
					const int32_t dst_y = cursor_tl_dst_y + rel_y;
					if (dst_x < 0 || dst_x >= m_framebuffer.width)
						continue;
					if (dst_y < 0 || dst_y >= m_framebuffer.height)
						continue;

					m_framebuffer.mmap[dst_y * m_framebuffer.width + dst_x] = pixel.value();
				}
			}

			mark_pending_sync(cursor_area);
		}

		return;
	}

	auto fb_overlap = area.get_overlap(m_framebuffer.area());
	if (!fb_overlap.has_value())
		return;
	area = fb_overlap.release_value();

	for (int32_t y = area.y; y < area.y + area.height; y++)
		for (int32_t x = area.x; x < area.x + area.width; x++)
			m_framebuffer.mmap[y * m_framebuffer.width + x] = m_background_image->get_color(x, y).as_argb();

	// FIXME: this loop should be inverse order and terminate
	//        after window without alpha channel is found
	for (auto& pwindow : m_client_windows)
	{
		auto& window = *pwindow;

		const Rectangle fast_areas[] {
			{
				window.full_x() + m_corner_radius,
				window.full_y(),
				window.full_width() - 2 * m_corner_radius,
				m_corner_radius
			},
			{
				window.full_x(),
				window.full_y() + m_corner_radius,
				window.full_width(),
				window.full_height() - 2 * m_corner_radius
			},
			{
				window.full_x() + m_corner_radius,
				window.full_y() + window.full_height() - m_corner_radius,
				window.full_width() - 2 * m_corner_radius,
				m_corner_radius
			}
		};

		const Position corner_centers[] {
			{
				window.full_x()                              + m_corner_radius,
				window.full_y()                              + m_corner_radius,
			},
			{
				window.full_x() + (window.full_width() - 1)  - m_corner_radius,
				window.full_y()                              + m_corner_radius,
			},
			{
				window.full_x()                              + m_corner_radius,
				window.full_y() + (window.full_height() - 1) - m_corner_radius,
			},
			{
				window.full_x() + (window.full_width()  - 1) - m_corner_radius,
				window.full_y() + (window.full_height() - 1) - m_corner_radius,
			},
		};

		const Rectangle corner_areas[] {
			{
				window.full_x(),
				window.full_y(),
				m_corner_radius,
				m_corner_radius
			},
			{
				window.full_x() + window.full_width() - m_corner_radius,
				window.full_y(),
				m_corner_radius,
				m_corner_radius
			},
			{
				window.full_x(),
				window.full_y() + window.full_height() - m_corner_radius,
				m_corner_radius,
				m_corner_radius
			},
			{
				window.full_x() + window.full_width() - m_corner_radius,
				window.full_y() + window.full_height() - m_corner_radius,
				m_corner_radius,
				m_corner_radius
			}
		};

		const auto is_rounded_off =
			[&](const Window& window, Position pos) -> bool
			{
				if (!window.get_attributes().rounded_corners)
					return false;
				for (int32_t i = 0; i < 4; i++)
				{
					if (!corner_areas[i].contains(pos))
						continue;
					const int32_t dx = pos.x - corner_centers[i].x;
					const int32_t dy = pos.y - corner_centers[i].y;
					if (2 * (dy > 0) + (dx > 0) != i)
						continue;
					if (dx * dx + dy * dy >= m_corner_radius * m_corner_radius)
						return true;
				}
				return false;
			};

		// window title bar
		if (auto title_overlap = window.title_bar_area().get_overlap(area); title_overlap.has_value())
		{
			for (int32_t y_off = 0; y_off < title_overlap->height; y_off++)
			{
				for (int32_t x_off = 0; x_off < title_overlap->width; x_off++)
				{
					const int32_t abs_x = title_overlap->x + x_off;
					const int32_t abs_y = title_overlap->y + y_off;
					if (is_rounded_off(window, { abs_x, abs_y }))
						continue;

					const uint32_t color = window.title_bar_pixel(abs_x, abs_y, m_cursor);
					m_framebuffer.mmap[abs_y * m_framebuffer.width + abs_x] = color;
				}
			}
		}

		// window client area
		if (auto client_overlap = window.client_area().get_overlap(area); client_overlap.has_value())
		{
			for (const auto& fast_area : fast_areas)
			{
				auto fast_overlap = client_overlap->get_overlap(fast_area);
				if (!fast_overlap.has_value())
					continue;
				for (int32_t y_off = 0; y_off < fast_overlap->height; y_off++)
				{
					const int32_t abs_row_y = fast_overlap->y + y_off;
					const int32_t abs_row_x = fast_overlap->x;

					const int32_t src_row_y = abs_row_y - window.client_y();
					const int32_t src_row_x = abs_row_x - window.client_x();

					auto* window_row = &window.framebuffer()[src_row_y * window.client_width() + src_row_x];
					auto* frameb_row = &m_framebuffer.mmap[  abs_row_y * m_framebuffer.width   + abs_row_x];

					const bool should_alpha_blend = window.get_attributes().alpha_channel;
					for (int32_t i = 0; i < fast_overlap->width; i++)
					{
						const uint32_t color_a = *window_row;
						const uint32_t color_b = *frameb_row;
						*frameb_row = should_alpha_blend
							? alpha_blend(color_a, color_b)
							: color_a;
						window_row++;
						frameb_row++;
					}
				}
			}

			for (const auto& corner_area : corner_areas)
			{
				auto corner_overlap = client_overlap->get_overlap(corner_area);
				if (!corner_overlap.has_value())
					continue;
				for (int32_t y_off = 0; y_off < corner_overlap->height; y_off++)
				{
					for (int32_t x_off = 0; x_off < corner_overlap->width; x_off++)
					{
						const int32_t abs_x = corner_overlap->x + x_off;
						const int32_t abs_y = corner_overlap->y + y_off;
						if (is_rounded_off(window, { abs_x, abs_y }))
							continue;

						const int32_t src_x = abs_x - window.client_x();
						const int32_t src_y = abs_y - window.client_y();

						const uint32_t color_a = window.framebuffer()[src_y * window.client_width() + src_x];
						const uint32_t color_b = m_framebuffer.mmap[abs_y * m_framebuffer.width + abs_x];

						const bool should_alpha_blend = window.get_attributes().alpha_channel;
						m_framebuffer.mmap[abs_y * m_framebuffer.width + abs_x] = should_alpha_blend
							? alpha_blend(color_a, color_b)
							: color_a;
					}
				}
			}
		}
	}

	if (m_state == State::Resizing)
	{
		if (const auto overlap = resize_area(m_cursor).get_overlap(area); overlap.has_value())
		{
			for (int32_t y_off = 0; y_off < overlap->height; y_off++)
			{
				for (int32_t x_off = 0; x_off < overlap->width; x_off++)
				{
					auto& pixel = m_framebuffer.mmap[(overlap->y + y_off) * m_framebuffer.width + (overlap->x + x_off)];
					pixel = alpha_blend(0x80000000, pixel);
				}
			}
		}
	}

	if (!m_is_mouse_captured)
	{
		if (const auto overlap = cursor_area().get_overlap(area); overlap.has_value())
		{
			for (int32_t y_off = 0; y_off < overlap->height; y_off++)
			{
				for (int32_t x_off = 0; x_off < overlap->width; x_off++)
				{
					const int32_t rel_x = overlap->x - m_cursor.x + x_off;
					const int32_t rel_y = overlap->y - m_cursor.y + y_off;
					const auto pixel = get_cursor_pixel(rel_x, rel_y);
					if (pixel.has_value())
						m_framebuffer.mmap[(overlap->y + y_off) * m_framebuffer.width + (overlap->x + x_off)] = pixel.value();
				}
			}
		}
	}

	mark_pending_sync(area);
}

void WindowServer::RangeList::add_range(const Range& range)
{
	if (range_count == 0)
	{
		ranges[0] = range;
		range_count++;
		return;
	}

	size_t min_distance_value = SIZE_MAX;
	size_t min_distance_index = 0;
	for (size_t i = 0; i < range_count; i++)
	{
		if (ranges[i].is_continuous_with(range))
		{
			ranges[i].merge_with(range);

			size_t last_continuous = i;
			for (size_t j = i + 1; j < range_count; j++)
			{
				if (!ranges[i].is_continuous_with(ranges[j]))
					break;
				last_continuous = j;
			}

			if (last_continuous != i)
			{
				ranges[i].merge_with(ranges[last_continuous]);
				for (size_t j = 1; last_continuous + j < range_count; j++)
					ranges[i + j] = ranges[last_continuous + j];
				range_count -= last_continuous - i;
			}

			return;
		}

		const auto distance = ranges[i].distance_between(range);
		if (distance < min_distance_value)
		{
			min_distance_value = distance;
			min_distance_index = i;
		}
	}

	if (range_count >= ranges.size())
	{
		ranges[min_distance_index].merge_with(range);
		return;
	}

	size_t insert_idx = 0;
	for (; insert_idx < range_count; insert_idx++)
		if (range.start < ranges[insert_idx].start)
			break;
	for (size_t i = range_count; i > insert_idx; i--)
		ranges[i] = ranges[i - 1];
	ranges[insert_idx] = range;
	range_count++;
}

void WindowServer::mark_pending_sync(Rectangle to_sync)
{
	ASSERT(to_sync == to_sync.get_overlap(m_framebuffer.area()).value());
	for (int32_t y_off = 0; y_off < to_sync.height; y_off++)
		m_pending_syncs[to_sync.y + y_off].add_range({ static_cast<uint32_t>(to_sync.x), static_cast<uint32_t>(to_sync.width) });
}

void WindowServer::sync()
{
	if (m_focused_window && m_is_bouncing_window)
	{
		static int32_t dir_x = 7;
		static int32_t dir_y = 4;
		auto old_window = m_focused_window->full_area();
		m_focused_window->set_position({
			m_focused_window->client_x() + dir_x,
			m_focused_window->client_y() + dir_y,
		});
		auto new_window = m_focused_window->full_area();
		invalidate(old_window);
		invalidate(new_window);

		if ((m_focused_window->full_x() < 0 && dir_x < 0) || (m_focused_window->full_x() + m_focused_window->full_width() >= m_framebuffer.width && dir_x > 0))
			dir_x = -dir_x;
		if ((m_focused_window->full_y() < 0 && dir_y < 0) || (m_focused_window->full_y() + m_focused_window->full_height() >= m_framebuffer.height && dir_y > 0))
			dir_y = -dir_y;
	}

	size_t range_start = 0;
	size_t range_count = 0;
	for (int32_t y = 0; y < m_framebuffer.height; y++)
	{
		auto& range_list = m_pending_syncs[y];

		for (size_t i = 0; i < range_list.range_count; i++)
		{
			const size_t cur_start = y * m_framebuffer.width + range_list.ranges[i].start;
			const size_t cur_count =                           range_list.ranges[i].count;

			if (range_count == 0)
			{
				range_start = cur_start;
				range_count = cur_count;
			}
			else
			{
				const size_t distance = cur_start - (range_start + range_count);

				// combine nearby ranges to reduce msync calls
				// NOTE: value of 128 is an arbitary constant that *just* felt nice
				if (distance <= 128)
					range_count = (cur_start + cur_count) - range_start;
				else
				{
					msync(m_framebuffer.mmap + range_start, range_count * 4, MS_SYNC);
					range_start = cur_start;
					range_count = cur_count;
				}
			}
		}

		range_list.range_count = 0;
	}

	if (range_count)
		msync(m_framebuffer.mmap + range_start, range_count * 4, MS_SYNC);
}

Rectangle WindowServer::cursor_area() const
{
	return { m_cursor.x, m_cursor.y, s_cursor_width, s_cursor_height };
}

Rectangle WindowServer::resize_area(Position cursor) const
{
	const auto min_size = m_focused_window->get_min_size();

	int32_t diff_x = m_resize_start.x - cursor.x;
	if (m_resize_quadrant % 2)
		diff_x = -diff_x;
	diff_x = BAN::Math::max(diff_x, -m_focused_window->client_width() + min_size.width);

	int32_t diff_y = m_resize_start.y - cursor.y;
	if (m_resize_quadrant / 2)
		diff_y = -diff_y;
	diff_y = BAN::Math::max(diff_y, -m_focused_window->client_height() + min_size.height);

	int32_t off_x = 0;
	if (m_resize_quadrant % 2 == 0)
		off_x = -diff_x;

	int32_t off_y = 0;
	if (m_resize_quadrant / 2 == 0)
		off_y = -diff_y;

	return {
		.x = off_x + m_focused_window->full_x(),
		.y = off_y + m_focused_window->full_y(),
		.width  = diff_x + m_focused_window->full_width(),
		.height = diff_y + m_focused_window->full_height(),
	};
}

bool WindowServer::resize_window(BAN::RefPtr<Window> window, uint32_t width, uint32_t height) const
{
	if (auto ret = window->resize(width, height); ret.is_error())
	{
		dwarnln("could not resize client window {}", ret.error());
		return false;
	}

	LibGUI::EventPacket::ResizeWindowEvent response;
	response.width = window->client_width();
	response.height = window->client_height();
	response.smo_key = window->smo_key();
	if (auto ret = response.send_serialized(window->client_fd()); ret.is_error())
	{
		dwarnln("could not respond to window resize request: {}", ret.error());
		return false;
	}

	return true;
}

void WindowServer::add_client_fd(int fd)
{
	if (auto ret = m_client_data.emplace(fd); ret.is_error())
	{
		dwarnln("could not add client: {}", ret.error());
		return;
	}
}

void WindowServer::remove_client_fd(int fd)
{
	auto it = m_client_data.find(fd);
	if (it == m_client_data.end())
		return;
	m_client_data.remove(it);

	if (m_state == State::Fullscreen && m_focused_window->client_fd() == fd)
	{
		m_state = State::Normal;
		invalidate(m_framebuffer.area());
	}

	for (size_t i = 0; i < m_client_windows.size(); i++)
	{
		auto window = m_client_windows[i];
		if (window->client_fd() == fd)
		{
			auto window_area = window->full_area();
			m_client_windows.remove(i);
			invalidate(window_area);

			if (window == m_focused_window)
			{
				m_focused_window = nullptr;
				for (size_t j = m_client_windows.size(); j > 0; j--)
				{
					auto& client_window = m_client_windows[j - 1];
					if (!client_window->get_attributes().focusable)
						continue;
					set_focused_window(client_window);
					break;
				}
			}

			break;
		}
	}

	m_deleted_window = true;
}

int WindowServer::get_client_fds(fd_set& fds) const
{
	int max_fd = 0;
	for (const auto& [fd, _] : m_client_data)
	{
		FD_SET(fd, &fds);
		max_fd = BAN::Math::max(max_fd, fd);
	}
	return max_fd;
}

void WindowServer::for_each_client_fd(const BAN::Function<BAN::Iteration(int, ClientData&)>& callback)
{
	m_deleted_window = false;
	for (auto& [fd, cliend_data] : m_client_data)
	{
		if (m_deleted_window)
			break;
		callback(fd, cliend_data);
	}
}
