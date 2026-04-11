#include "Cursor.h"
#include "WindowServer.h"

#include <BAN/Debug.h>
#include <BAN/ScopeGuard.h>

#include <LibGUI/Window.h>
#include <LibInput/KeyboardLayout.h>

#include <stdlib.h>
#include <sys/banan-os.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#include <emmintrin.h>

WindowServer::WindowServer(Framebuffer& framebuffer, int32_t corner_radius)
	: m_framebuffer(framebuffer)
	, m_corner_radius(corner_radius)
	, m_cursor({ framebuffer.width / 2, framebuffer.height / 2 })
	, m_font(MUST(LibFont::Font::load("/usr/share/fonts/lat0-16.psfu"_sv)))
{
	MUST(m_background_image.resize(m_framebuffer.width * m_framebuffer.height, 0xFF101010));

	add_damaged_area(m_framebuffer.area());
}

BAN::ErrorOr<void> WindowServer::set_background_image(BAN::UniqPtr<LibImage::Image> image)
{
	if (image->width() != (uint64_t)m_framebuffer.width || image->height() != (uint64_t)m_framebuffer.height)
		image = TRY(image->resize(m_framebuffer.width, m_framebuffer.height));

	for (int32_t y = 0; y < m_framebuffer.height; y++)
		for (int32_t x = 0; x < m_framebuffer.width; x++)
			m_background_image[y * m_framebuffer.width + x] = image->get_color(x, y).as_argb();

	add_damaged_area(m_framebuffer.area());
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

	const LibGUI::EventPacket::ResizeWindowEvent event_packet {
		.width  = static_cast<uint32_t>(window->client_width()),
		.height = static_cast<uint32_t>(window->client_height()),
		.smo_key = window->smo_key(),
	};
	if (auto ret = append_serialized_packet(event_packet, fd); ret.is_error())
	{
		dwarnln("could not respond to window create request: {}", ret.error());
		return;
	}

	window_popper.disable();

	if (packet.attributes.shown && packet.attributes.focusable && m_state == State::Normal)
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

	auto target_window = find_window_with_fd(fd);
	if (!target_window)
	{
		dwarnln("client tried to invalidate window while not owning a window");
		return;
	}

	if (!target_window->get_attributes().shown)
		return;

	const auto client_area = target_window->client_area();

	const Rectangle damaged_area {
		.min_x = client_area.min_x + static_cast<int32_t>(packet.x),
		.min_y = client_area.min_y + static_cast<int32_t>(packet.y),
		.max_x = client_area.min_x + static_cast<int32_t>(packet.x + packet.width),
		.max_y = client_area.min_y + static_cast<int32_t>(packet.y + packet.height),
	};

	if (auto opt_overlap = damaged_area.get_overlap(client_area); opt_overlap.has_value())
		add_damaged_area(opt_overlap.release_value());
}

void WindowServer::on_window_set_position(int fd, const LibGUI::WindowPacket::WindowSetPosition& packet)
{
	if (m_state == State::Fullscreen)
	{
		ASSERT(m_focused_window);
		if (m_focused_window->client_fd() != fd)
			return;
	}

	auto target_window = find_window_with_fd(fd);
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

	if (!target_window->get_attributes().shown)
		return;

	add_damaged_area(old_client_area);
	add_damaged_area(target_window->full_area());
}

void WindowServer::on_window_set_attributes(int fd, const LibGUI::WindowPacket::WindowSetAttributes& packet)
{
	auto target_window = find_window_with_fd(fd);
	if (!target_window)
	{
		dwarnln("client tried to set window attributes while not owning a window");
		return;
	}

	const bool send_shown_event = target_window->get_attributes().shown != packet.attributes.shown;

	const auto old_client_area = target_window->full_area();
	target_window->set_attributes(packet.attributes);

	add_damaged_area(old_client_area);
	add_damaged_area(target_window->full_area());

	if ((!packet.attributes.focusable || !packet.attributes.shown) && m_focused_window == target_window)
	{
		if (m_state == State::Fullscreen && m_focused_window->get_attributes().resizable)
		{
			if (!resize_window(m_focused_window, m_non_full_screen_rect.width(), m_non_full_screen_rect.height()))
				return;
			m_focused_window->set_position({ m_non_full_screen_rect.min_x, m_non_full_screen_rect.min_y });
		}

		m_focused_window = nullptr;
		m_state = State::Normal;
		for (size_t i = m_client_windows.size(); i > 0; i--)
		{
			auto& window = m_client_windows[i - 1];
			if (auto attributes = window->get_attributes(); attributes.focusable && attributes.shown)
			{
				set_focused_window(window);
				break;
			}
		}
	}

	if (!send_shown_event)
		return;

	const LibGUI::EventPacket::WindowShownEvent event_packet { .event = {
		.shown = target_window->get_attributes().shown,
	}};
	if (auto ret = append_serialized_packet(event_packet, target_window->client_fd()); ret.is_error())
		dwarnln("could not send window shown event: {}", ret.error());

	if (packet.attributes.focusable && packet.attributes.shown && m_state == State::Normal)
		set_focused_window(target_window);
}

void WindowServer::on_window_set_mouse_relative(int fd, const LibGUI::WindowPacket::WindowSetMouseRelative& packet)
{
	if (m_is_mouse_relative && packet.enabled)
	{
		ASSERT(m_focused_window);
		if (fd != m_focused_window->client_fd())
			dwarnln("client tried to set mouse relative while other window has it already");
		return;
	}

	auto target_window = find_window_with_fd(fd);
	if (!target_window)
	{
		dwarnln("client tried to set mouse capture while not owning a window");
		return;
	}
	if (!target_window->get_attributes().shown)
	{
		dwarnln("client tried to set mouse capture while hidden window");
		return;
	}

	if (packet.enabled == m_is_mouse_relative)
		return;

	set_focused_window(target_window);
	m_is_mouse_relative = packet.enabled;
	add_damaged_area(cursor_area());
}

void WindowServer::on_window_set_size(int fd, const LibGUI::WindowPacket::WindowSetSize& packet)
{
	auto target_window = find_window_with_fd(fd);
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

	if (!target_window->get_attributes().shown)
		return;

	add_damaged_area(old_area);
	add_damaged_area(target_window->full_area());
}

void WindowServer::on_window_set_min_size(int fd, const LibGUI::WindowPacket::WindowSetMinSize& packet)
{
	auto target_window = find_window_with_fd(fd);
	if (!target_window)
	{
		dwarnln("client tried to set window min size while not owning a window");
		return;
	}

	// FIXME: should this resize window
	target_window->set_min_size({ 0, 0, static_cast<int32_t>(packet.width), static_cast<int32_t>(packet.height) });
}

void WindowServer::on_window_set_max_size(int fd, const LibGUI::WindowPacket::WindowSetMaxSize& packet)
{
	auto target_window = find_window_with_fd(fd);
	if (!target_window)
	{
		dwarnln("client tried to set window max size while not owning a window");
		return;
	}

	// FIXME: should this resize window
	target_window->set_max_size({ 0, 0, static_cast<int32_t>(packet.width), static_cast<int32_t>(packet.height) });
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
			if (!resize_window(m_focused_window, m_non_full_screen_rect.width(), m_non_full_screen_rect.height()))
				return;
			m_focused_window->set_position({ m_non_full_screen_rect.min_x, m_non_full_screen_rect.min_y });
		}

		const LibGUI::EventPacket::WindowFullscreenEvent event_packet { .event = {
			.fullscreen = false,
		}};
		if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send window fullscreen event: {}", ret.error());

		m_state = State::Normal;
		add_damaged_area(m_framebuffer.area());
		return;
	}

	if (!packet.fullscreen)
		return;

	auto target_window = find_window_with_fd(fd);
	if (!target_window)
	{
		dwarnln("client tried to set window fullscreen while not owning a window");
		return;
	}
	if (!target_window->get_attributes().shown)
	{
		dwarnln("client tried to set a hidden window fullscreen");
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

	const LibGUI::EventPacket::WindowFullscreenEvent event_packet { .event = {
		.fullscreen = true,
	}};
	if (auto ret = append_serialized_packet(event_packet, target_window->client_fd()); ret.is_error())
		dwarnln("could not send window fullscreen event: {}", ret.error());

	m_state = State::Fullscreen;
	set_focused_window(target_window);
	add_damaged_area(m_framebuffer.area());
}

void WindowServer::on_window_set_title(int fd, const LibGUI::WindowPacket::WindowSetTitle& packet)
{
	auto target_window = find_window_with_fd(fd);
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

	if (!target_window->get_attributes().shown)
		return;

	add_damaged_area(target_window->title_text_area());
}

void WindowServer::on_window_set_cursor(int fd, const LibGUI::WindowPacket::WindowSetCursor& packet)
{
	auto target_window = find_window_with_fd(fd);
	if (!target_window)
	{
		dwarnln("client tried to set cursor while not owning a window");
		return;
	}

	if (BAN::Math::will_multiplication_overflow(packet.width, packet.height))
	{
		dwarnln("client tried to set cursor with invalid size {}x{}", packet.width, packet.height);
		return;
	}

	if (packet.width * packet.height != packet.pixels.size())
	{
		dwarnln("client tried to set cursor with buffer size mismatch {}x{}, {} pixels", packet.width, packet.height, packet.pixels.size());
		return;
	}

	const auto old_cursor_area = cursor_area();

	if (packet.width == 0 || packet.height == 0)
		target_window->remove_cursor();
	else
	{
		Window::Cursor cursor {
			.width = packet.width,
			.height = packet.height,
			.origin_x = packet.origin_x,
			.origin_y = packet.origin_y,
			.pixels = {},
		};
		if (auto ret = cursor.pixels.resize(packet.pixels.size()); ret.is_error())
		{
			dwarnln("failed to set cursor: {}", ret.error());
			return;
		}
		for (size_t i = 0; i < cursor.pixels.size(); i++)
			cursor.pixels[i] = packet.pixels[i];
		target_window->set_cursor(BAN::move(cursor));
	}

	if (find_hovered_window() == target_window)
	{
		add_damaged_area(old_cursor_area);
		add_damaged_area(cursor_area());
	}
}

static void update_volume(const char* new_volume)
{
	char command[128];
	sprintf(command, "audioctl --volume %s && kill -USR1 TaskBar", new_volume);
	system(command);
}

void WindowServer::on_key_event(LibInput::KeyEvent event)
{
	if (event.key == LibInput::Key::Super)
		m_is_mod_key_held = event.pressed();

	if (event.pressed() && event.key == LibInput::Key::VolumeDown)
		update_volume("-5");
	if (event.pressed() && event.key == LibInput::Key::VolumeUp)
		update_volume("+5");

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

	// Start program launcher with mod+d
	if (m_is_mod_key_held && event.pressed() && event.key == LibInput::Key::D)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			execl("/usr/bin/ProgramLauncher", "ProgramLauncher", nullptr);
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
		const LibGUI::EventPacket::CloseWindowEvent event_packet {};
		if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send window close event: {}", ret.error());
		return;
	}

	if (m_is_mod_key_held && event.pressed() && event.key == LibInput::Key::F)
	{
		if (m_state == State::Fullscreen)
		{
			if (m_focused_window->get_attributes().resizable)
			{
				if (!resize_window(m_focused_window, m_non_full_screen_rect.width(), m_non_full_screen_rect.height()))
					return;
				m_focused_window->set_position({ m_non_full_screen_rect.min_x, m_non_full_screen_rect.min_y });
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

		const LibGUI::EventPacket::WindowFullscreenEvent event_packet { .event = {
			.fullscreen = (m_state == State::Fullscreen),
		}};
		if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send window fullscreen event: {}", ret.error());

		add_damaged_area(m_framebuffer.area());
		return;
	}

	const LibGUI::EventPacket::KeyEvent event_packet {
		.event = event,
	};
	if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
		dwarnln("could not send key event: {}", ret.error());
}

void WindowServer::on_mouse_button(LibInput::MouseButtonEvent event)
{
	if (m_is_mouse_relative)
	{
		ASSERT(m_focused_window);

		const LibGUI::EventPacket::MouseButtonEvent event_packet { .event = {
			.button = event.button,
			.pressed = event.pressed,
			.x = 0,
			.y = 0,
		}};
		if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send mouse button event: {}", ret.error());
		return;
	}

	const size_t button_idx = static_cast<size_t>(event.button);
	if (button_idx >= m_mouse_button_windows.size())
	{
		dwarnln("invalid mouse button {}", button_idx);
		return;
	}

	BAN::RefPtr<Window> target_window;
	if (m_state == State::Fullscreen)
		target_window = m_focused_window;
	if (!event.pressed && !target_window)
		target_window = m_mouse_button_windows[button_idx];
	for (size_t i = m_client_windows.size(); i > 0 && !target_window; i--)
		if (m_client_windows[i - 1]->full_area().contains(m_cursor) && m_client_windows[i - 1]->get_attributes().shown)
			target_window = m_client_windows[i - 1];

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
				const LibGUI::EventPacket::CloseWindowEvent event_packet {};
				if (auto ret = append_serialized_packet(event_packet, target_window->client_fd()); ret.is_error())
					dwarnln("could not send close window event: {}", ret.error());
				break;
			}

			[[fallthrough]];
		case State::Fullscreen:
			if (target_window && (!event.pressed || target_window->client_area().contains(m_cursor)))
			{
				const LibGUI::EventPacket::MouseButtonEvent event_packet { .event = {
					.button = event.button,
					.pressed = event.pressed,
					.x = m_cursor.x - target_window->client_x(),
					.y = m_cursor.y - target_window->client_y(),
				}};
				if (auto ret = append_serialized_packet(event_packet, target_window->client_fd()); ret.is_error())
				{
					dwarnln("could not send mouse button event: {}", ret.error());
					return;
				}
				m_mouse_button_windows[button_idx] = event.pressed ? target_window : nullptr;
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

				add_damaged_area(resize_area);
				add_damaged_area(m_focused_window->full_area());

				if (auto ret = m_focused_window->resize(resize_area.width(), resize_area.height() - m_focused_window->title_bar_height()); ret.is_error())
				{
					dwarnln("could not resize client window {}", ret.error());
					return;
				}
				m_focused_window->set_position({ resize_area.min_x, resize_area.min_y + m_focused_window->title_bar_height() });

				const LibGUI::EventPacket::ResizeWindowEvent event_packet {
					.width  = static_cast<uint32_t>(m_focused_window->client_width()),
					.height = static_cast<uint32_t>(m_focused_window->client_height()),
					.smo_key = m_focused_window->smo_key(),
				};
				if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
				{
					dwarnln("could not respond to window resize request: {}", ret.error());
					return;
				}

				add_damaged_area(m_focused_window->full_area());
			}
			break;
	}
}

void WindowServer::on_mouse_move_impl(int32_t new_x, int32_t new_y)
{
	const LibInput::MouseMoveEvent event {
		.rel_x = new_x - m_cursor.x,
		.rel_y = new_y - m_cursor.y,
	};
	if (event.rel_x == 0 && event.rel_y == 0)
		return;

	const auto old_cursor_area = cursor_area();
	m_cursor.x = new_x;
	m_cursor.y = new_y;

	add_damaged_area(old_cursor_area);
	add_damaged_area(cursor_area());

	// TODO: Really no need to loop over every window
	for (auto& window : m_client_windows)
	{
		if (!window->get_attributes().shown)
			continue;
		const auto title_bar_area = window->title_bar_area();
		if (title_bar_area.get_overlap(old_cursor_area).has_value() || title_bar_area.get_overlap(cursor_area()).has_value())
			add_damaged_area(title_bar_area);
	}

	if (!m_focused_window)
		return;

	switch (m_state)
	{
		case State::Normal:
		case State::Fullscreen:
		{
			const LibGUI::EventPacket::MouseMoveEvent event_packet { .event = {
				.x = m_cursor.x - m_focused_window->client_x(),
				.y = m_cursor.y - m_focused_window->client_y(),
			}};
			if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
			{
				dwarnln("could not send mouse move event: {}", ret.error());
				return;
			}
			break;
		}
		case State::Moving:
		{
			const auto old_window_area = m_focused_window->full_area();
			m_focused_window->set_position({
				m_focused_window->client_x() + event.rel_x,
				m_focused_window->client_y() + event.rel_y,
			});
			add_damaged_area(old_window_area);
			add_damaged_area(m_focused_window->full_area());
			break;
		}
		case State::Resizing:
		{
			add_damaged_area(resize_area({ .x = old_cursor_area.min_x, .y = old_cursor_area.min_y }));
			add_damaged_area(resize_area(m_cursor));
			break;
		}
	}
}

void WindowServer::on_mouse_move(LibInput::MouseMoveEvent event)
{
	if (m_is_mouse_relative)
	{
		ASSERT(m_focused_window);

		const LibGUI::EventPacket::MouseMoveEvent event_packet { .event = {
			.x =  event.rel_x,
			.y = -event.rel_y,
		}};
		if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send mouse move event: {}", ret.error());
		return;
	}

	int32_t min_x, max_x;
	int32_t min_y, max_y;

	if (m_state == State::Fullscreen)
	{
		min_x = m_focused_window->client_x();
		min_y = m_focused_window->client_y();
		max_x = m_focused_window->client_x() + m_focused_window->client_width();
		max_y = m_focused_window->client_y() + m_focused_window->client_height();
	}
	else
	{
		min_x = 0;
		min_y = 0;
		max_x = m_framebuffer.width;
		max_y = m_framebuffer.height;
	}

	const int32_t new_x = BAN::Math::clamp(m_cursor.x + event.rel_x, min_x, max_x);
	const int32_t new_y = BAN::Math::clamp(m_cursor.y - event.rel_y, min_y, max_y);
	return on_mouse_move_impl(new_x, new_y);
}

void WindowServer::on_mouse_move_abs(LibInput::MouseMoveAbsEvent event)
{
	constexpr auto map =
		[](int32_t val, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) -> int32_t
		{
			return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
		};

	if (m_is_mouse_relative)
	{
		dwarnln("relative mouse not supported with absolute mouse");
		return;
	}

	int32_t out_min_x, out_max_x;
	int32_t out_min_y, out_max_y;

	if (m_state == State::Fullscreen)
	{
		out_min_x = m_focused_window->client_x();
		out_min_y = m_focused_window->client_y();
		out_max_x = m_focused_window->client_x() + m_focused_window->client_width();
		out_max_y = m_focused_window->client_y() + m_focused_window->client_height();
	}
	else
	{
		out_min_x = 0;
		out_min_y = 0;
		out_max_x = m_framebuffer.width;
		out_max_y = m_framebuffer.height;
	}

	const int32_t new_x = map(event.abs_x, event.min_x, event.max_x, out_min_x, out_max_x);
	const int32_t new_y = map(event.abs_y, event.min_y, event.max_y, out_min_y, out_max_y);
	return on_mouse_move_impl(new_x, new_y);
}

void WindowServer::on_mouse_scroll(LibInput::MouseScrollEvent event)
{
	if (!m_focused_window)
		return;

	const LibGUI::EventPacket::MouseScrollEvent event_packet { .event = {
		.scroll = event.scroll,
	}};
	if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
	{
		dwarnln("could not send mouse scroll event: {}", ret.error());
		return;
	}
}

void WindowServer::set_focused_window(BAN::RefPtr<Window> window)
{
	ASSERT(window->get_attributes().focusable);
	if (m_focused_window == window)
		return;

	if (m_is_mouse_relative)
	{
		m_is_mouse_relative = false;
		add_damaged_area(cursor_area());
	}

	if (m_focused_window)
	{
		const LibGUI::EventPacket::WindowFocusEvent event_packet { .event = {
			.focused = false,
		}};
		if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send window focus event: {}", ret.error());
	}

	for (size_t i = m_client_windows.size(); i > 0; i--)
	{
		if (m_client_windows[i - 1] == window)
		{
			m_focused_window = window;
			m_client_windows.remove(i - 1);
			MUST(m_client_windows.push_back(window));
			add_damaged_area(window->full_area());
			break;
		}
	}

	if (m_focused_window)
	{
		const LibGUI::EventPacket::WindowFocusEvent event_packet { .event = {
			.focused = true,
		}};
		if (auto ret = append_serialized_packet(event_packet, m_focused_window->client_fd()); ret.is_error())
			dwarnln("could not send window focus event: {}", ret.error());
	}
}

static uint32_t alpha_blend(uint32_t color_a, uint32_t color_b)
{
	const uint32_t a_a = color_a >> 24;
	const uint32_t a_b = 0xFF - a_a;

	const uint32_t rb1 = (a_a * (color_a & 0xFF00FF)) >> 8;
	const uint32_t rb2 = (a_b * (color_b & 0xFF00FF)) >> 8;

	const uint32_t g1  = (a_a * (color_a & 0x00FF00)) >> 8;
	const uint32_t g2  = (a_b * (color_b & 0x00FF00)) >> 8;

    const uint32_t a = a_a + (((color_b >> 24) * a_b) >> 8);
	return (a << 24) | ((rb1 + rb2) & 0xFF00FF) | ((g1 + g2) & 0x00FF00);
}

static void alpha_blend4(const uint32_t color_a[4], const uint32_t color_b[4], uint32_t color_out[4])
{
	// load colors
	const __m128i ca = _mm_loadu_si128((const __m128i*)color_a);
	const __m128i cb = _mm_loadu_si128((const __m128i*)color_b);

	// unpack colors to 16 bit words
	const __m128i zero = _mm_setzero_si128();
	const __m128i ca_lo = _mm_unpacklo_epi8(ca, zero);
	const __m128i ca_hi = _mm_unpackhi_epi8(ca, zero);
	const __m128i cb_lo = _mm_unpacklo_epi8(cb, zero);
	const __m128i cb_hi = _mm_unpackhi_epi8(cb, zero);

	// extract alpha channel from color_a
	const __m128i a1_lo = _mm_shufflehi_epi16(_mm_shufflelo_epi16(ca_lo, 0b11'11'11'11), 0b11'11'11'11);
	const __m128i a1_hi = _mm_shufflehi_epi16(_mm_shufflelo_epi16(ca_hi, 0b11'11'11'11), 0b11'11'11'11);

	// calculate inverse alpha
	const __m128i low_byte16 = _mm_set1_epi16(0xFF);
	const __m128i a2_lo = _mm_sub_epi16(low_byte16, a1_lo);
	const __m128i a2_hi = _mm_sub_epi16(low_byte16, a1_hi);

	// blend and pack rgb (a*c1 + c2*(255-a)) / 256
	const __m128i rgb_lo = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(ca_lo, a1_lo), _mm_mullo_epi16(cb_lo, a2_lo)), 8);
	const __m128i rgb_hi = _mm_srli_epi16(_mm_add_epi16(_mm_mullo_epi16(ca_hi, a1_hi), _mm_mullo_epi16(cb_hi, a2_hi)), 8);
	const __m128i rgb = _mm_and_si128(_mm_set1_epi32(0x00FFFFFF), _mm_packus_epi16(rgb_lo, rgb_hi));

	// extract alpha channel from color_b
	const __m128i ab_lo = _mm_shufflehi_epi16(_mm_shufflelo_epi16(cb_lo, 0b11'11'11'11), 0b11'11'11'11);
	const __m128i ab_hi = _mm_shufflehi_epi16(_mm_shufflelo_epi16(cb_hi, 0b11'11'11'11), 0b11'11'11'11);

	// blend and pack alpha (a + ab*(255-a)) / 256
	const __m128i alpha_lo = _mm_add_epi16(a1_lo, _mm_srli_epi16(_mm_mullo_epi16(ab_lo, a2_lo), 8));
	const __m128i alpha_hi = _mm_add_epi16(a1_hi, _mm_srli_epi16(_mm_mullo_epi16(ab_hi, a2_hi), 8));
	const __m128i alpha = _mm_slli_epi32(_mm_packus_epi16(alpha_lo, alpha_hi), 24);

	_mm_storeu_si128((__m128i*)color_out, _mm_or_si128(alpha, rgb));
}

void WindowServer::invalidate(Rectangle area)
{
	const Window::Cursor* window_cursor = nullptr;
	if (auto window = this->find_hovered_window(); window && window->has_cursor())
		window_cursor = &window->cursor();

	const auto get_cursor_pixel =
		[window_cursor](int32_t rel_x, int32_t rel_y) -> BAN::Optional<uint32_t>
		{
			if (window_cursor)
			{
				const auto pixel = window_cursor->pixels[rel_y * window_cursor->width + rel_x];
				if ((pixel >> 24) == 0)
					return {};
				return pixel & 0xFFFFFF;
			}

			const uint32_t offset = (rel_y * s_default_cursor_width + rel_x) * 4;
			const uint32_t r = ((( s_default_cursor_data[offset + 0] - 33       ) << 2) | ((s_default_cursor_data[offset + 1] - 33) >> 4));
			const uint32_t g = ((((s_default_cursor_data[offset + 1] - 33) & 0xF) << 4) | ((s_default_cursor_data[offset + 2] - 33) >> 2));
			const uint32_t b = ((((s_default_cursor_data[offset + 2] - 33) & 0x3) << 6) | ((s_default_cursor_data[offset + 3] - 33)     ));
			const uint32_t color = (r << 16) | (g << 8) | b;
			if (color == 0xFF00FF)
				return {};
			return color;
		};

	if (m_state == State::Fullscreen)
	{
		ASSERT(m_focused_window);
		area.min_x -= m_focused_window->client_x();
		area.max_x -= m_focused_window->client_x();
		area.min_y -= m_focused_window->client_y();
		area.max_y -= m_focused_window->client_y();

		const Rectangle client_area {
			.min_x = 0,
			.min_y = 0,
			.max_x = m_focused_window->client_width(),
			.max_y = m_focused_window->client_height(),
		};

		auto focused_overlap = area.get_overlap(client_area);
		if (!focused_overlap.has_value())
			return;
		area = focused_overlap.release_value();

		const bool should_alpha_blend = m_focused_window->get_attributes().alpha_channel;

		if (client_area == m_framebuffer.area())
		{
			const uint32_t* client_ptr = m_focused_window->framebuffer();
			const size_t client_width = m_focused_window->client_width();

			if (!should_alpha_blend)
			{
				for (int32_t y = area.min_y; y < area.max_y; y++)
				{
					memcpy(
						&m_framebuffer.mmap[y * m_framebuffer.width + area.min_x],
						&client_ptr[y * client_width + area.min_x],
						area.width() * sizeof(uint32_t)
					);
				}
			}
			else
			{
				for (int32_t y = area.min_y; y < area.max_y; y++)
				{
					const uint32_t* window_row = &client_ptr[y * client_width + area.min_x];
					const uint32_t* image_row  = &m_background_image[y * m_framebuffer.width + area.min_x];
					uint32_t* frameb_row = &m_framebuffer.mmap[y * m_framebuffer.width + area.min_x];

					int32_t pixels = area.width();
					for (; pixels >= 4; pixels -= 4)
					{
						alpha_blend4(window_row, image_row, frameb_row);
						frameb_row += 4;
						window_row += 4;
						image_row += 4;
					}
					for (; pixels > 0; pixels--)
					{
						*frameb_row = alpha_blend(*window_row, *image_row);
						frameb_row++;
						window_row++;
						image_row++;
					}
				}
			}
		}
		else
		{
			auto opt_dst_area = Rectangle {
				.min_x = area.min_x * m_framebuffer.width  / m_focused_window->client_width(),
				.min_y = area.min_y * m_framebuffer.height / m_focused_window->client_height(),
				.max_x = BAN::Math::div_round_up(area.max_x * m_framebuffer.width,  m_focused_window->client_width()),
				.max_y = BAN::Math::div_round_up(area.max_y * m_framebuffer.height, m_focused_window->client_height())
			}.get_overlap(m_framebuffer.area());
			if (!opt_dst_area.has_value())
				return;

			const auto dst_area = opt_dst_area.release_value();
			for (int32_t dst_y = dst_area.min_y; dst_y < dst_area.max_y; dst_y++)
			{
				for (int32_t dst_x = dst_area.min_x; dst_x < dst_area.max_x; dst_x++)
				{
					const int32_t src_x = BAN::Math::clamp<int32_t>(dst_x * m_focused_window->client_width()  / m_framebuffer.width,  0, m_focused_window->client_width());
					const int32_t src_y = BAN::Math::clamp<int32_t>(dst_y * m_focused_window->client_height() / m_framebuffer.height, 0, m_focused_window->client_height());

					const uint32_t src_pixel = m_focused_window->framebuffer()[src_y * m_focused_window->client_width() + src_x];
					const uint32_t bg_pixel = m_background_image[dst_y * m_framebuffer.width + dst_x];

					uint32_t& dst_pixel = m_framebuffer.mmap[dst_y * m_framebuffer.width + dst_x];
					dst_pixel = should_alpha_blend ? alpha_blend(src_pixel, bg_pixel) : src_pixel;
				}
			}
		}

		if (!m_is_mouse_relative)
		{
			auto cursor_area = this->cursor_area();
			cursor_area.min_x -= m_focused_window->client_x();
			cursor_area.max_x -= m_focused_window->client_x();
			cursor_area.min_y -= m_focused_window->client_y();
			cursor_area.max_y -= m_focused_window->client_y();
			if (!area.get_overlap(cursor_area).has_value())
				return;

			const int32_t cursor_tl_dst_x = cursor_area.min_x * m_framebuffer.width  / m_focused_window->client_width();
			const int32_t cursor_tl_dst_y = cursor_area.min_y * m_framebuffer.height / m_focused_window->client_height();

			for (int32_t rel_y = 0; rel_y < cursor_area.height(); rel_y++)
			{
				for (int32_t rel_x = 0; rel_x < cursor_area.width(); rel_x++)
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
		}

		return;
	}

	auto fb_overlap = area.get_overlap(m_framebuffer.area());
	if (!fb_overlap.has_value())
		return;
	area = fb_overlap.release_value();

	for (int32_t y = area.min_y; y < area.max_y; y++)
	{
		memcpy(
			&m_framebuffer.mmap[y * m_framebuffer.width + area.min_x],
			&m_background_image[y * m_framebuffer.width + area.min_x],
			area.width() * sizeof(uint32_t)
		);
	}

	// FIXME: this loop should be inverse order and terminate
	//        after window without alpha channel is found
	for (auto& pwindow : m_client_windows)
	{
		auto& window = *pwindow;
		if (!window.get_attributes().shown)
			continue;

		const auto window_full_area = window.full_area();

		const Rectangle fast_areas[] {
			{
				.min_x = window_full_area.min_x,
				.min_y = window_full_area.min_y + m_corner_radius,
				.max_x = window_full_area.max_x,
				.max_y = window_full_area.max_y - m_corner_radius,
			}, {
				.min_x = window_full_area.min_x + m_corner_radius,
				.min_y = window_full_area.min_y,
				.max_x = window_full_area.max_x - m_corner_radius,
				.max_y = window_full_area.min_y + m_corner_radius,
			}, {
				.min_x = window_full_area.min_x + m_corner_radius,
				.min_y = window_full_area.max_y - m_corner_radius,
				.max_x = window_full_area.max_x - m_corner_radius,
				.max_y = window_full_area.max_y,
			},
		};

		const Position corner_centers[] {
			{
				.x = window_full_area.min_x + m_corner_radius,
				.y = window_full_area.min_y + m_corner_radius,
			}, {
				.x = window_full_area.max_x - m_corner_radius - 1,
				.y = window_full_area.min_y + m_corner_radius,
			}, {
				.x = window_full_area.min_x + m_corner_radius,
				.y = window_full_area.max_y - m_corner_radius - 1,
			}, {
				.x = window_full_area.max_x - m_corner_radius - 1,
				.y = window_full_area.max_y - m_corner_radius - 1,
			},
		};

		const Rectangle corner_areas[] {
			{
				.min_x = window_full_area.min_x,
				.min_y = window_full_area.min_y,
				.max_x = window_full_area.min_x + m_corner_radius,
				.max_y = window_full_area.min_y + m_corner_radius,
			}, {
				.min_x = window_full_area.max_x - m_corner_radius,
				.min_y = window_full_area.min_y,
				.max_x = window_full_area.max_x,
				.max_y = window_full_area.min_y + m_corner_radius,
			}, {
				.min_x = window_full_area.min_x,
				.min_y = window_full_area.max_y - m_corner_radius,
				.max_x = window_full_area.min_x + m_corner_radius,
				.max_y = window_full_area.max_y,
			}, {
				.min_x = window_full_area.max_x - m_corner_radius,
				.min_y = window_full_area.max_y - m_corner_radius,
				.max_x = window_full_area.max_x,
				.max_y = window_full_area.max_y,
			},
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
		if (auto opt_title_overlap = window.title_bar_area().get_overlap(area); opt_title_overlap.has_value())
		{
			const auto title_overlap = opt_title_overlap.release_value();
			for (int32_t abs_y = title_overlap.min_y; abs_y < title_overlap.max_y; abs_y++)
			{
				for (int32_t abs_x = title_overlap.min_x; abs_x < title_overlap.max_x; abs_x++)
				{
					if (is_rounded_off(window, { abs_x, abs_y }))
						continue;

					m_framebuffer.mmap[abs_y * m_framebuffer.width + abs_x] =
						window.title_bar_pixel(abs_x, abs_y, m_cursor);
				}
			}
		}

		// window client area
		if (auto opt_client_overlap = window.client_area().get_overlap(area); opt_client_overlap.has_value())
		{
			const bool should_alpha_blend = window.get_attributes().alpha_channel;

			const auto client_overlap = opt_client_overlap.release_value();
			for (const auto& fast_area : fast_areas)
			{
				auto opt_fast_overlap = client_overlap.get_overlap(fast_area);
				if (!opt_fast_overlap.has_value())
					continue;

				const auto fast_overlap = opt_fast_overlap.release_value();
				for (int32_t abs_row_y = fast_overlap.min_y; abs_row_y < fast_overlap.max_y; abs_row_y++)
				{
					const int32_t abs_row_x = fast_overlap.min_x;

					const int32_t src_row_y = abs_row_y - window.client_y();
					const int32_t src_row_x = abs_row_x - window.client_x();

					auto* window_row = &window.framebuffer()[src_row_y * window.client_width() + src_row_x];
					auto* frameb_row = &m_framebuffer.mmap[abs_row_y * m_framebuffer.width + abs_row_x];

					if (!should_alpha_blend)
						memcpy(frameb_row, window_row, fast_overlap.width() * sizeof(uint32_t));
					else
					{
						int32_t pixels = fast_overlap.width();
						for (; pixels >= 4; pixels -= 4)
						{
							alpha_blend4(window_row, frameb_row, frameb_row);
							window_row += 4;
							frameb_row += 4;
						}
						for (; pixels; pixels--)
						{
							*frameb_row = alpha_blend(*window_row, *frameb_row);
							window_row++;
							frameb_row++;
						}
					}
				}
			}

			for (const auto& corner_area : corner_areas)
			{
				auto opt_corner_overlap = client_overlap.get_overlap(corner_area);
				if (!opt_corner_overlap.has_value())
					continue;

				const auto corner_overlap = opt_corner_overlap.release_value();
				for (int32_t abs_y = corner_overlap.min_y; abs_y < corner_overlap.max_y; abs_y++)
				{
					for (int32_t abs_x = corner_overlap.min_x; abs_x < corner_overlap.max_x; abs_x++)
					{
						if (is_rounded_off(window, { abs_x, abs_y }))
							continue;

						const int32_t src_x = abs_x - window.client_x();
						const int32_t src_y = abs_y - window.client_y();

						const uint32_t color_a = window.framebuffer()[src_y * window.client_width() + src_x];
						const uint32_t color_b = m_framebuffer.mmap[abs_y * m_framebuffer.width + abs_x];

						// NOTE: corners are small so we do them one pixel at a time to keep the code simple
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
		if (auto opt_overlap = resize_area(m_cursor).get_overlap(area); opt_overlap.has_value())
		{
			constexpr uint32_t blend_color = 0x80000000;

			const auto overlap = opt_overlap.release_value();
			for (int32_t y = overlap.min_y; y < overlap.max_y; y++)
			{
				uint32_t* frameb_row = &m_framebuffer.mmap[y * m_framebuffer.width + overlap.min_x];

				int32_t pixels = overlap.width();
				for (; pixels >= 4; pixels -= 4)
				{
					const uint32_t blend_colors[] { blend_color, blend_color, blend_color, blend_color };
					alpha_blend4(blend_colors, frameb_row, frameb_row);
					frameb_row += 4;
				}
				for (; pixels > 0; pixels--)
				{
					*frameb_row = alpha_blend(blend_color, *frameb_row);
					frameb_row++;
				}
			}
		}
	}

	if (!m_is_mouse_relative)
	{
		if (auto opt_overlap = cursor_area().get_overlap(area); opt_overlap.has_value())
		{
			const int32_t origin_x = window_cursor ? window_cursor->origin_x : 0;
			const int32_t origin_y = window_cursor ? window_cursor->origin_y : 0;
			const auto overlap = opt_overlap.release_value();
			for (int32_t abs_y = overlap.min_y; abs_y < overlap.max_y; abs_y++)
			{
				for (int32_t abs_x = overlap.min_x; abs_x < overlap.max_x; abs_x++)
				{
					const int32_t rel_x = abs_x - m_cursor.x + origin_x;
					const int32_t rel_y = abs_y - m_cursor.y + origin_y;
					const auto pixel = get_cursor_pixel(rel_x, rel_y);
					if (pixel.has_value())
						m_framebuffer.mmap[abs_y * m_framebuffer.width + abs_x] = pixel.value();
				}
			}
		}
	}
}

void WindowServer::merge_damaged_areas()
{
	constexpr size_t max_unique_coords = m_max_damaged_areas * 2;

	BAN::Array<int32_t, max_unique_coords> collapsed_x, collapsed_y;
	uint32_t bitmap[(max_unique_coords * max_unique_coords + 31) / 32];

	static constexpr auto get_collapsed_index =
		[](const BAN::Array<int32_t, max_unique_coords>& container, size_t container_size, int32_t value) -> size_t
		{
			int32_t l = 0, r = container_size - 1;
			while (l <= r)
			{
				const int32_t mid = l + (r - l) / 2;
				if (container[mid] == value)
					return mid;
				(container[mid] < value)
					? l = mid + 1
					: r = mid - 1;
			}
			return l;
		};

	static constexpr auto collapse_value =
		[](BAN::Array<int32_t, max_unique_coords>& container, size_t& container_size, int32_t value) -> void
		{
			const size_t index = get_collapsed_index(container, container_size, value);
			if (index < container_size && container[index] == value)
				return;
			for (size_t i = container_size; i > index; i--)
				container[i] = container[i - 1];
			container[index] = value;
			container_size++;
		};

	size_t collapsed_x_size = 0, collapsed_y_size = 0;
	for (size_t i = 0; i < m_damaged_area_count; i++)
	{
		collapse_value(collapsed_x, collapsed_x_size, m_damaged_areas[i].min_x);
		collapse_value(collapsed_x, collapsed_x_size, m_damaged_areas[i].max_x);
		collapse_value(collapsed_y, collapsed_y_size, m_damaged_areas[i].min_y);
		collapse_value(collapsed_y, collapsed_y_size, m_damaged_areas[i].max_y);
	}

	const auto is_bitmap_bit_set =
		[&](size_t x, size_t y) -> bool
		{
			const size_t index = y * max_unique_coords + x;
			return (bitmap[index / 32] >> (index % 32)) & 1;
		};

	const auto set_bitmap_bit =
		[&](size_t x, size_t y, bool set) -> void
		{
			const size_t index = y * max_unique_coords + x;
			const uint32_t mask = static_cast<uint32_t>(1) << (index % 32);
			set ? bitmap[index / 32] |=  mask
				: bitmap[index / 32] &= ~mask;
		};

	memset(bitmap, 0, sizeof(bitmap));
	for (size_t i = 0; i < m_damaged_area_count; i++)
	{
		const size_t cmin_x = get_collapsed_index(collapsed_x, collapsed_x_size, m_damaged_areas[i].min_x);
		const size_t cmax_x = get_collapsed_index(collapsed_x, collapsed_x_size, m_damaged_areas[i].max_x);
		const size_t cmin_y = get_collapsed_index(collapsed_y, collapsed_y_size, m_damaged_areas[i].min_y);
		const size_t cmax_y = get_collapsed_index(collapsed_y, collapsed_y_size, m_damaged_areas[i].max_y);
		for (size_t y = cmin_y; y < cmax_y; y++)
			for (size_t x = cmin_x; x < cmax_x; x++)
				set_bitmap_bit(x, y, true);
	}

	BAN::Array<Rectangle, m_max_damaged_areas> new_rectangles;
	size_t new_rectangle_count = 0;

	for (size_t min_y = 0; min_y < collapsed_y_size; min_y++)
	{
		for (size_t min_x = 0; min_x < collapsed_x_size; min_x++)
		{
			const size_t index = min_y * max_unique_coords + min_x;
			if (index % 32 == 0 && bitmap[index / 32] == 0)
			{
				min_x += 31;
				continue;
			}

			if (!is_bitmap_bit_set(min_x, min_y))
				continue;

			size_t max_x = min_x + 1;
			while (max_x < collapsed_x_size && is_bitmap_bit_set(max_x, min_y))
				max_x++;

			size_t max_y = min_y + 1;
			while (max_y < collapsed_y_size)
			{
				bool all_bits_set = true;
				for (size_t x = min_x; x < max_x && all_bits_set; x++)
					all_bits_set = is_bitmap_bit_set(x, max_y);
				if (!all_bits_set)
					break;
				max_y++;
			}

			new_rectangles[new_rectangle_count++] = {
				.min_x = collapsed_x[min_x],
				.min_y = collapsed_y[min_y],
				.max_x = collapsed_x[max_x],
				.max_y = collapsed_y[max_y],
			};
			if (new_rectangle_count >= m_damaged_area_count)
				return;

			for (size_t y = min_y; y < max_y; y++)
				for (size_t x = min_x; x < max_x; x++)
					set_bitmap_bit(x, y, false);
		}
	}

	memcpy(
		m_damaged_areas.data(),
		new_rectangles.data(),
		new_rectangle_count * sizeof(Rectangle)
	);
	m_damaged_area_count = new_rectangle_count;
}

void WindowServer::add_damaged_area_impl(Rectangle new_rect)
{
	if (new_rect.area() == 0)
		return;

	const auto remove_rectangle =
		[this](size_t index)
		{
			memmove(
				m_damaged_areas.data() + index,
				m_damaged_areas.data() + index + 1,
				(m_damaged_area_count - index - 1) * sizeof(Rectangle)
			);
			m_damaged_area_count--;
		};

	// handle trivial cases where `new_rect` contains or is contained by another rectangle
	for (size_t i = 0; i < m_damaged_area_count; i++)
	{
		auto opt_overlap = m_damaged_areas[i].get_overlap(new_rect);
		if (!opt_overlap.has_value())
			continue;
		const auto overlap = opt_overlap.value();
		if (overlap == new_rect)
			return;
		if (overlap == m_damaged_areas[i])
			remove_rectangle(i--);
	}

	// split `new_rect` into smaller pieces if there are overlapping rectangles
	for (size_t i = 0; i < m_damaged_area_count; i++)
	{
		auto opt_overlap = m_damaged_areas[i].get_overlap(new_rect);
		if (!opt_overlap.has_value())
			continue;
		const auto overlap = opt_overlap.value();

		Rectangle splitted[9];

		size_t count = new_rect.split_along_edges_of(m_damaged_areas[i], splitted);
		if (count == 6)
		{
			count = m_damaged_areas[i].split_along_edges_of(new_rect, splitted);
			remove_rectangle(i);
			add_damaged_area_impl(new_rect);
		}

		for (size_t j = 0; j < count; j++)
			if (splitted[j] != overlap)
				add_damaged_area_impl(splitted[j]);
		return;
	}

	// combine existing rectangles if needed
	if (m_damaged_area_count >= m_damaged_areas.size())
		merge_damaged_areas();

	// insert `new_rect` to the set of rectangles
	if (m_damaged_area_count < m_damaged_areas.size())
	{
		m_damaged_areas[m_damaged_area_count++] = new_rect;
		return;
	}

	const auto get_bounding_box_without_overlap =
		[this](const Rectangle& a, const Rectangle& b) -> Rectangle
		{
			auto bounding_box = a.get_bounding_box(b);

			for (;;)
			{
				bool did_update = false;
				for (size_t i = 0; i < m_damaged_area_count; i++)
				{
					const auto opt_overlap = m_damaged_areas[i].get_overlap(bounding_box);
					if (!opt_overlap.has_value() || opt_overlap.value() == m_damaged_areas[i])
						continue;
					bounding_box = bounding_box.get_bounding_box(m_damaged_areas[i]);
					did_update = true;
				}

				if (!did_update)
					return bounding_box;
			}
		};

	// find the rectangles whose bounding box with this adds least area and does not overlap with other rectangles

	int32_t min_overflow = BAN::numeric_limits<int32_t>::max();
	Rectangle min_overflow_bb = {};

	for (size_t i = 0; i < m_damaged_area_count; i++)
	{
		for (size_t j = i + 1; j < m_damaged_area_count; j++)
		{
			const auto bounding_box = get_bounding_box_without_overlap(m_damaged_areas[i], m_damaged_areas[j]);

			int32_t overflow = bounding_box.area();
			for (size_t k = 0; k < m_damaged_area_count; k++)
				overflow -= bounding_box.get_overlap(m_damaged_areas[k]).value_or({}).area();
			overflow -= bounding_box.get_overlap(new_rect).value_or({}).area();

			if (overflow >= min_overflow)
				continue;
			min_overflow = overflow;
			min_overflow_bb = bounding_box;
		}
	}

	add_damaged_area_impl(min_overflow_bb);
	add_damaged_area_impl(new_rect);
}

void WindowServer::add_damaged_area(Rectangle new_rect)
{
	auto opt_fb_overlap = new_rect.get_overlap(m_framebuffer.area());
	if (!opt_fb_overlap.has_value())
		return;
	add_damaged_area_impl(opt_fb_overlap.release_value());
	merge_damaged_areas();
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

		add_damaged_area(old_window);
		add_damaged_area(m_focused_window->full_area());

		if ((m_focused_window->full_x() < 0 && dir_x < 0) || (m_focused_window->full_x() + m_focused_window->full_width() >= m_framebuffer.width && dir_x > 0))
			dir_x = -dir_x;
		if ((m_focused_window->full_y() < 0 && dir_y < 0) || (m_focused_window->full_y() + m_focused_window->full_height() >= m_framebuffer.height && dir_y > 0))
			dir_y = -dir_y;
	}

	for (size_t i = 0; i < m_damaged_area_count; i++)
		invalidate(m_damaged_areas[i]);

	for (size_t i = 0; i < m_damaged_area_count; i++)
	{
		const fb_msync_region region {
			.min_x = static_cast<uint32_t>(m_damaged_areas[i].min_x),
			.min_y = static_cast<uint32_t>(m_damaged_areas[i].min_y),
			.max_x = static_cast<uint32_t>(m_damaged_areas[i].max_x),
			.max_y = static_cast<uint32_t>(m_damaged_areas[i].max_y),
		};
		ioctl(m_framebuffer.fd, FB_MSYNC_RECTANGLE, &region);
	}

	m_damaged_area_count = 0;
}

Rectangle WindowServer::cursor_area() const
{
	int32_t width = s_default_cursor_width;
	int32_t height = s_default_cursor_height;

	int32_t origin_x = 0;
	int32_t origin_y = 0;

	if (auto window = find_hovered_window())
	{
		if (!window->get_attributes().cursor_visible)
			width = height = 0;
		else if (window->has_cursor())
		{
			width = window->cursor().width;
			height = window->cursor().height;
			origin_x = window->cursor().origin_x;
			origin_y = window->cursor().origin_y;
		}
	}

	return Rectangle {
		.min_x = m_cursor.x - origin_x,
		.min_y = m_cursor.y - origin_y,
		.max_x = m_cursor.x - origin_x + width,
		.max_y = m_cursor.y - origin_y + height,
	};
}

Rectangle WindowServer::resize_area(Position cursor) const
{
	const auto min_size = m_focused_window->get_min_size();
	const auto max_size = m_focused_window->get_max_size();

	int32_t diff_x = m_resize_start.x - cursor.x;
	if (m_resize_quadrant % 2)
		diff_x = -diff_x;
	diff_x = BAN::Math::clamp(diff_x,
		-m_focused_window->client_width() + min_size.width(),
		-m_focused_window->client_width() + max_size.width()
	);

	int32_t diff_y = m_resize_start.y - cursor.y;
	if (m_resize_quadrant / 2)
		diff_y = -diff_y;
	diff_y = BAN::Math::clamp(diff_y,
		-m_focused_window->client_height() + min_size.height(),
		-m_focused_window->client_height() + max_size.height()
	);

	int32_t off_x = 0;
	if (m_resize_quadrant % 2 == 0)
		off_x = -diff_x;

	int32_t off_y = 0;
	if (m_resize_quadrant / 2 == 0)
		off_y = -diff_y;

	const int min_x = off_x + m_focused_window->full_x();
	const int min_y = off_y + m_focused_window->full_y();
	return Rectangle {
		.min_x = min_x,
		.min_y = min_y,
		.max_x = min_x + diff_x + m_focused_window->full_width(),
		.max_y = min_y + diff_y + m_focused_window->full_height(),
	};
}

BAN::RefPtr<Window> WindowServer::find_window_with_fd(int fd) const
{
	for (auto window : m_client_windows)
		if (window->client_fd() == fd)
			return window;
	return {};
}

BAN::RefPtr<Window> WindowServer::find_hovered_window() const
{
	for (size_t i = m_client_windows.size(); i > 0; i--)
		if (auto window = m_client_windows[i - 1]; window->client_area().contains(m_cursor))
			return window;
	return {};
}

bool WindowServer::resize_window(BAN::RefPtr<Window> window, uint32_t width, uint32_t height)
{
	if (auto ret = window->resize(width, height); ret.is_error())
	{
		dwarnln("could not resize client window {}", ret.error());
		return false;
	}

	const LibGUI::EventPacket::ResizeWindowEvent event_packet {
		.width  = static_cast<uint32_t>(window->client_width()),
		.height = static_cast<uint32_t>(window->client_height()),
		.smo_key = window->smo_key(),
	};
	if (auto ret = append_serialized_packet(event_packet, window->client_fd()); ret.is_error())
	{
		dwarnln("could not respond to window resize request: {}", ret.error());
		return false;
	}

	return true;
}

BAN::ErrorOr<void> WindowServer::add_client_fd(int fd)
{
	TRY(m_client_data.emplace(fd));
	return {};
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
		add_damaged_area(m_framebuffer.area());
	}

	for (auto& window : m_mouse_button_windows)
		if (window && window->client_fd() == fd)
			window.clear();

	for (size_t i = 0; i < m_client_windows.size(); i++)
	{
		auto window = m_client_windows[i];
		if (window->client_fd() == fd)
		{
			auto window_area = window->full_area();
			m_client_windows.remove(i);
			add_damaged_area(window_area);

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
}

WindowServer::ClientData& WindowServer::get_client_data(int fd)
{
	auto it = m_client_data.find(fd);
	if (it != m_client_data.end())
		return it->value;

	dwarnln("could not find client {}", fd);
	for (auto& [client_fd, _] : m_client_data)
		dwarnln("  {}", client_fd);

	ASSERT_NOT_REACHED();
}

// TODO: this epoll stuff is very hacky

#include <sys/epoll.h>

extern int g_epoll_fd;

template<typename T>
BAN::ErrorOr<void> WindowServer::append_serialized_packet(const T& packet, int fd)
{
	const size_t serialized_size = packet.serialized_size();

	auto& client_data = m_client_data[fd];
	if (client_data.out_buffer_size + serialized_size > client_data.out_buffer.size())
		return BAN::Error::from_errno(ENOBUFS);

	if (client_data.out_buffer_size == 0)
	{
		epoll_event event { .events = EPOLLIN | EPOLLOUT, .data = { .fd = fd } };
		if (epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1)
			dwarnln("epoll_ctl add EPOLLOUT: {}", strerror(errno));
	}

	packet.serialize(client_data.out_buffer.span().slice(client_data.out_buffer_size, serialized_size));
	client_data.out_buffer_size += serialized_size;

	return {};
}
