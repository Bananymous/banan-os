#include "Cursor.h"
#include "WindowServer.h"

#include <BAN/Debug.h>

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
	MUST(m_pages_to_sync_bitmap.resize(BAN::Math::div_round_up<size_t>(m_framebuffer.width * m_framebuffer.height * sizeof(uint32_t), 4096 * 8), 0));
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

void WindowServer::on_window_packet(int fd, LibGUI::WindowPacket packet)
{
	switch (packet.type)
	{
		case LibGUI::WindowPacketType::CreateWindow:
		{
			// FIXME: This should be probably allowed
			for (auto& window : m_client_windows)
			{
				if (window->client_fd() == fd)
				{
					dwarnln("client {} tried to create window while already owning a window", fd);
					return;
				}
			}

			const size_t window_fb_bytes = packet.create.width * packet.create.height * 4;

			long smo_key = smo_create(window_fb_bytes, PROT_READ | PROT_WRITE);
			if (smo_key == -1)
			{
				dwarnln("smo_create: {}", strerror(errno));
				break;
			}

			Rectangle window_area {
				static_cast<int32_t>((m_framebuffer.width - packet.create.width) / 2),
				static_cast<int32_t>((m_framebuffer.height - packet.create.height) / 2),
				static_cast<int32_t>(packet.create.width),
				static_cast<int32_t>(packet.create.height)
			};

			packet.create.title[sizeof(packet.create.title) - 1] = '\0';

			// Window::Window(int fd, Rectangle area, long smo_key, BAN::StringView title, const LibFont::Font& font)
			auto window = MUST(BAN::RefPtr<Window>::create(
				fd,
				window_area,
				smo_key,
				packet.create.title,
				m_font
			));
			MUST(m_client_windows.push_back(window));
			set_focused_window(window);

			LibGUI::WindowCreateResponse response;
			response.framebuffer_smo_key = smo_key;
			if (send(window->client_fd(), &response, sizeof(response), 0) != sizeof(response))
			{
				dwarnln("send: {}", strerror(errno));
				break;
			}

			break;
		}
		case LibGUI::WindowPacketType::Invalidate:
		{
			if (packet.invalidate.width == 0 || packet.invalidate.height == 0)
				break;

			BAN::RefPtr<Window> target_window;
			for (auto& window : m_client_windows)
			{
				if (window->client_fd() == fd)
				{
					target_window = window;
					break;
				}
			}
			if (!target_window)
			{
				dwarnln("client {} tried to invalidate window while not owning a window", fd);
				break;
			}

			const int32_t br_x = packet.invalidate.x + packet.invalidate.width - 1;
			const int32_t br_y = packet.invalidate.y + packet.invalidate.height - 1;
			if (!target_window->client_size().contains({ br_x, br_y }))
			{
				dwarnln("Invalid Invalidate packet parameters");
				break;
			}

			invalidate({
				target_window->client_x() + static_cast<int32_t>(packet.invalidate.x),
				target_window->client_y() + static_cast<int32_t>(packet.invalidate.y),
				static_cast<int32_t>(packet.invalidate.width),
				static_cast<int32_t>(packet.invalidate.height),
			});

			break;
		}
		default:
			ASSERT_NOT_REACHED();
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

	// Kill window with mod+Q
	if (m_is_mod_key_held && event.pressed() && event.key == LibInput::Key::Q)
	{
		if (m_focused_window)
			remove_client_fd(m_focused_window->client_fd());
		return;
	}

	// Toggle window bounce with F2
	if (event.pressed() && event.key == LibInput::Key::F2)
		m_is_bouncing_window = !m_is_bouncing_window;

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
	for (size_t i = m_client_windows.size(); i > 0; i--)
	{
		if (m_client_windows[i - 1]->full_area().contains(m_cursor))
		{
			target_window = m_client_windows[i - 1];
			break;
		}
	}

	// Ignore mouse button events which are not on top of a window
	if (!target_window)
		return;

	set_focused_window(target_window);

	// Handle window moving when mod key is held or mouse press on title bar
	const bool can_start_move = m_is_mod_key_held || target_window->title_text_area().contains(m_cursor);
	if (event.pressed && event.button == LibInput::MouseButton::Left && !m_is_moving_window && can_start_move)
		m_is_moving_window = true;
	else if (m_is_moving_window && !event.pressed)
		m_is_moving_window = false;
	else if (!event.pressed && event.button == LibInput::MouseButton::Left && target_window->close_button_area().contains(m_cursor))
	{
		// NOTE: we always have target window if code reaches here
		LibGUI::EventPacket packet;
		packet.type = LibGUI::EventPacket::Type::CloseWindow;
		send(m_focused_window->client_fd(), &packet, sizeof(packet), 0);
	}
	else if (target_window->client_area().contains(m_cursor))
	{
		// NOTE: we always have target window if code reaches here
		LibGUI::EventPacket packet;
		packet.type = LibGUI::EventPacket::Type::MouseButtonEvent;
		packet.mouse_button_event.button = event.button;
		packet.mouse_button_event.pressed = event.pressed;
		packet.mouse_button_event.x = m_cursor.x - m_focused_window->client_x();
		packet.mouse_button_event.y = m_cursor.y - m_focused_window->client_y();
		send(m_focused_window->client_fd(), &packet, sizeof(packet), 0);
	}
}

void WindowServer::on_mouse_move(LibInput::MouseMoveEvent event)
{
	const int32_t new_x = BAN::Math::clamp(m_cursor.x + event.rel_x, 0, m_framebuffer.width);
	const int32_t new_y = BAN::Math::clamp(m_cursor.y - event.rel_y, 0, m_framebuffer.height);

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

	if (m_is_moving_window)
	{
		auto old_window = m_focused_window->full_area();
		m_focused_window->set_position({
			m_focused_window->client_x() + event.rel_x,
			m_focused_window->client_y() + event.rel_y,
		});
		auto new_window = m_focused_window->full_area();
		invalidate(old_window);
		invalidate(new_window);
		return;
	}

	if (m_focused_window)
	{
		LibGUI::EventPacket packet;
		packet.type = LibGUI::EventPacket::Type::MouseMoveEvent;
		packet.mouse_move_event.x = m_cursor.x - m_focused_window->client_x();
		packet.mouse_move_event.y = m_cursor.y - m_focused_window->client_y();
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
	auto fb_overlap = area.get_overlap(m_framebuffer.area());
	if (!fb_overlap.has_value())
		return;
	area = fb_overlap.release_value();

	if (m_background_image)
	{
		ASSERT(m_background_image->width() == (uint64_t)m_framebuffer.width);
		ASSERT(m_background_image->height() == (uint64_t)m_framebuffer.height);
		for (int32_t y = area.y; y < area.y + area.height; y++)
			for (int32_t x = area.x; x < area.x + area.width; x++)
				m_framebuffer.mmap[y * m_framebuffer.width + x] = m_background_image->get_color(x, y).as_rgba();
	}
	else
	{
		for (int32_t y = area.y; y < area.y + area.height; y++)
			for (int32_t x = area.x; x < area.x + area.width; x++)
				m_framebuffer.mmap[y * m_framebuffer.width + x] = 0xFF101010;
	}

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
			[&](Position pos) -> bool
			{
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
					if (is_rounded_off({ abs_x, abs_y }))
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

					for (int32_t i = 0; i < fast_overlap->width; i++)
					{
						const uint32_t color_a = *window_row;
						const uint32_t color_b = *frameb_row;
						*frameb_row = alpha_blend(color_a, color_b);

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
						if (is_rounded_off({ abs_x, abs_y }))
							continue;

						const int32_t src_x = abs_x - window.client_x();
						const int32_t src_y = abs_y - window.client_y();

						const uint32_t color_a = window.framebuffer()[src_y * window.client_width() + src_x];
						const uint32_t color_b = m_framebuffer.mmap[abs_y * m_framebuffer.width + abs_x];

						m_framebuffer.mmap[abs_y * m_framebuffer.width + abs_x] = alpha_blend(color_a, color_b);
					}
				}
			}
		}
	}

	auto cursor = cursor_area();
	if (auto overlap = cursor.get_overlap(area); overlap.has_value())
	{
		for (int32_t y_off = 0; y_off < overlap->height; y_off++)
		{
			for (int32_t x_off = 0; x_off < overlap->width; x_off++)
			{
				const int32_t rel_x = overlap->x - m_cursor.x + x_off;
				const int32_t rel_y = overlap->y - m_cursor.y + y_off;
				const uint32_t offset = (rel_y * s_cursor_width + rel_x) * 4;
				uint32_t r = (((s_cursor_data[offset + 0] - 33) << 2) | ((s_cursor_data[offset + 1] - 33) >> 4));
				uint32_t g = ((((s_cursor_data[offset + 1] - 33) & 0xF) << 4) | ((s_cursor_data[offset + 2] - 33) >> 2));
				uint32_t b = ((((s_cursor_data[offset + 2] - 33) & 0x3) << 6) | ((s_cursor_data[offset + 3] - 33)));
				uint32_t color = (r << 16) | (g << 8) | b;
				if (color != 0xFF00FF)
					m_framebuffer.mmap[(overlap->y + y_off) * m_framebuffer.width + (overlap->x + x_off)] = color;
			}
		}
	}

	const uintptr_t mmap_start = reinterpret_cast<uintptr_t>(m_framebuffer.mmap) + area.y * m_framebuffer.width * 4;
	const uintptr_t mmap_end = mmap_start + (area.height + 1) * m_framebuffer.width * 4;

	uintptr_t mmap_addr = mmap_start & ~(uintptr_t)0xFFF;
	while (mmap_addr < mmap_end)
	{
		size_t index = (mmap_addr - reinterpret_cast<uintptr_t>(m_framebuffer.mmap)) / 4096;
		size_t byte = index / 8;
		size_t bit  = index % 8;
		m_pages_to_sync_bitmap[byte] |= 1 << bit;
		mmap_addr += 4096;
	}
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

	for (size_t i = 0; i < m_pages_to_sync_bitmap.size() * 8; i++)
	{
		size_t byte = i / 8;
		size_t bit  = i % 8;
		if (!(m_pages_to_sync_bitmap[byte] & (1 << bit)))
			continue;

		size_t len = 1;
		while (i + len < m_pages_to_sync_bitmap.size() * 8)
		{
			size_t byte = (i + len) / 8;
			size_t bit  = (i + len) % 8;
			if (!(m_pages_to_sync_bitmap[byte] & (1 << bit)))
				break;
			len++;
		}

		msync(
			reinterpret_cast<uint8_t*>(m_framebuffer.mmap) + i * 4096,
			len * 4096,
			MS_SYNC
		);

		i += len;
	}

	memset(m_pages_to_sync_bitmap.data(), 0, m_pages_to_sync_bitmap.size());
}

Rectangle WindowServer::cursor_area() const
{
	return { m_cursor.x, m_cursor.y, s_cursor_width, s_cursor_height };
}


void WindowServer::add_client_fd(int fd)
{
	MUST(m_client_fds.push_back(fd));
}

void WindowServer::remove_client_fd(int fd)
{
	for (size_t i = 0; i < m_client_fds.size(); i++)
	{
		if (m_client_fds[i] == fd)
		{
			m_client_fds.remove(i);
			break;
		}
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
				if (!m_client_windows.empty())
					set_focused_window(m_client_windows.back());
			}

			break;
		}
	}

	m_deleted_window = true;
}

int WindowServer::get_client_fds(fd_set& fds) const
{
	int max_fd = 0;
	for (int fd : m_client_fds)
	{
		FD_SET(fd, &fds);
		max_fd = BAN::Math::max(max_fd, fd);
	}
	return max_fd;
}

void WindowServer::for_each_client_fd(const BAN::Function<BAN::Iteration(int)>& callback)
{
	m_deleted_window = false;
	for (int fd : m_client_fds)
	{
		if (m_deleted_window)
			break;
		callback(fd);
	}
}
