#include <LibGUI/Window.h>

#include <BAN/ScopeGuard.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <emmintrin.h>

namespace LibGUI
{

	Window::~Window()
	{
		cleanup();
	}

	BAN::ErrorOr<BAN::UniqPtr<Window>> Window::create(uint32_t width, uint32_t height, BAN::StringView title, Attributes attributes)
	{
		const auto get_current_ms = []() -> uint64_t {
			timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			return ts.tv_sec * 1'000 + ts.tv_nsec / 1'000'000;
		};

		int server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
		if (server_fd == -1)
			return BAN::Error::from_errno(errno);
		BAN::ScopeGuard server_closer([server_fd] { close(server_fd); });

		int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
		if (epoll_fd == -1)
			return BAN::Error::from_errno(errno);
		BAN::ScopeGuard epoll_closer([epoll_fd] { close(epoll_fd); });

		epoll_event epoll_event { .events = EPOLLIN, .data = { .fd = server_fd } };
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &epoll_event) == -1)
			return BAN::Error::from_errno(errno);

		const uint64_t timeout_ms = get_current_ms() + 1000;

		for (;;)
		{
			sockaddr_un server_address;
			server_address.sun_family = AF_UNIX;
			strcpy(server_address.sun_path, s_window_server_socket.data());
			if (connect(server_fd, (sockaddr*)&server_address, sizeof(server_address)) == 0)
				break;

			const uint64_t current_ms = get_current_ms();
			if (current_ms > timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

			const timespec sleep_time {
				.tv_sec = 0,
				.tv_nsec = 1'000'000,
			};
			nanosleep(&sleep_time, nullptr);
		}

		auto window = TRY(BAN::UniqPtr<Window>::create(server_fd, epoll_fd, attributes));

		WindowPacket::WindowCreate create_packet {
			.width = width,
			.height = height,
			.attributes = attributes,
			.title = {},
		};
		TRY(create_packet.title.append(title));
		window->send_packet(create_packet, __FUNCTION__);

		window->m_initializing = true;
		while (window->m_initializing)
		{
			const uint64_t current_ms = get_current_ms();
			if (current_ms > timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

			const uint64_t wait_ms = timeout_ms - current_ms;
			const timespec timeout {
				.tv_sec = static_cast<time_t>(wait_ms / 1000),
				.tv_nsec = static_cast<long>((wait_ms % 1000) * 1'000'000),
			};

			window->wait_events(&timeout);
			window->poll_events();

			if (window->m_init_failed)
				return BAN::Error::from_literal("failed to initialize window");
		}

		server_closer.disable();
		epoll_closer.disable();

		return window;
	}

	template<typename T>
	void Window::send_packet(const T& packet, BAN::StringView function)
	{
		const size_t serialized_size = packet.serialized_size();
		if (serialized_size > m_out_buffer.size())
		{
			dwarnln("cannot to send {} byte packet", serialized_size);
			return on_socket_error(function);
		}

		packet.serialize(m_out_buffer.span());

		size_t total_sent = 0;
		while (total_sent < serialized_size)
		{
			const ssize_t nsend = send(m_server_fd, m_out_buffer.data() + total_sent, serialized_size - total_sent, 0);
			if (nsend < 0)
				dwarnln("send: {}", strerror(errno));
			if (nsend <= 0)
				return on_socket_error(function);
			total_sent += nsend;
		}
	}

	BAN::ErrorOr<void> Window::set_root_widget(BAN::RefPtr<Widget::Widget> widget)
	{
		TRY(widget->set_fixed_geometry({ 0, 0, m_width, m_height }));
		m_root_widget = widget;
		m_root_widget->show();
		const auto invalidated = m_root_widget->render(m_texture, { 0, 0 }, { 0, 0, m_width, m_height });
		if (invalidated.w && invalidated.h)
			invalidate(invalidated.x, invalidated.y, invalidated.w, invalidated.h);
		return {};
	}

	static void* copy_pixels_and_set_max_alpha(void* dst, const void* src, size_t bytes)
	{
		size_t pixels = bytes / sizeof(uint32_t);

		      __m128i* dst128 = static_cast<      __m128i*>(dst);
		const __m128i* src128 = static_cast<const __m128i*>(src);
		const __m128i alpha_mask = _mm_set1_epi32(0xFF000000);
		for (; pixels >= 4; pixels -= 4)
			_mm_storeu_si128(dst128++, _mm_or_si128(_mm_loadu_si128(src128++), alpha_mask));

		      uint32_t* dst32 = reinterpret_cast<      uint32_t*>(dst128);
		const uint32_t* src32 = reinterpret_cast<const uint32_t*>(src128);
		for (; pixels; pixels--)
			*dst32++ = *src32++ | 0xFF000000;

		return nullptr;
	}

	void Window::invalidate(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		if (!m_texture.clamp_to_texture(x, y, width, height))
			return;

		const auto copy_func = m_attributes.alpha_channel ? memcpy : copy_pixels_and_set_max_alpha;

		if (width == m_width)
		{
			copy_func(
				&m_framebuffer_shm[y * m_width],
				&m_texture.pixels()[y * m_width],
				width * height * sizeof(uint32_t)
			);
		}
		else for (uint32_t y_off = 0; y_off < height; y_off++)
		{
			copy_func(
				&m_framebuffer_shm[(y + y_off) * m_width + x],
				&m_texture.pixels()[(y + y_off) * m_width + x],
				width * sizeof(uint32_t)
			);
		}

		const WindowPacket::WindowInvalidate packet {
			.x = static_cast<uint32_t>(x),
			.y = static_cast<uint32_t>(y),
			.width = width,
			.height = height,
		};
		send_packet(packet, __FUNCTION__);
	}

	void Window::set_mouse_relative(bool enabled)
	{
		const WindowPacket::WindowSetMouseRelative packet {
			.enabled = enabled,
		};
		send_packet(packet, __FUNCTION__);
	}

	void Window::set_fullscreen(bool fullscreen)
	{
		const WindowPacket::WindowSetFullscreen packet {
			.fullscreen = fullscreen,
		};
		send_packet(packet, __FUNCTION__);
	}

	void Window::set_title(BAN::StringView title)
	{
		WindowPacket::WindowSetTitle packet {
			.title = {},
		};
		MUST(packet.title.append(title));
		send_packet(packet, __FUNCTION__);
	}

	void Window::set_position(int32_t x, int32_t y)
	{
		const WindowPacket::WindowSetPosition packet {
			.x = x,
			.y = y,
		};
		send_packet(packet, __FUNCTION__);
	}

	void Window::set_cursor_visible(bool visible)
	{
		auto attributes = m_attributes;
		if (attributes.cursor_visible == visible)
			return;
		attributes.cursor_visible = visible;
		set_attributes(attributes);
	}

	void Window::set_cursor(uint32_t width, uint32_t height, BAN::Span<const uint32_t> pixels, int32_t origin_x, int32_t origin_y)
	{
		// TODO: no need to copy here
		WindowPacket::WindowSetCursor packet {
			.width = width,
			.height = height,
			.origin_x = origin_x,
			.origin_y = origin_y,
			.pixels = {},
		};
		MUST(packet.pixels.resize(pixels.size()));
		for (size_t i = 0; i < packet.pixels.size(); i++)
			packet.pixels[i] = pixels[i];
		send_packet(packet, __FUNCTION__);
	}

	void Window::set_min_size(uint32_t width, uint32_t height)
	{
		const WindowPacket::WindowSetMinSize packet {
			.width = width,
			.height = height,
		};
		send_packet(packet, __FUNCTION__);
	}

	void Window::set_max_size(uint32_t width, uint32_t height)
	{
		const WindowPacket::WindowSetMaxSize packet {
			.width = width,
			.height = height,
		};
		send_packet(packet, __FUNCTION__);
	}

	void Window::set_attributes(Attributes attributes)
	{
		WindowPacket::WindowSetAttributes packet;
		packet.attributes = attributes;
		send_packet(packet, __FUNCTION__);
		m_attributes = attributes;
	}

	void Window::request_resize(uint32_t width, uint32_t height)
	{
		const WindowPacket::WindowSetSize packet {
			.width = width,
			.height = height,
		};
		send_packet(packet, __FUNCTION__);
	}

	void Window::query_cursor_position()
	{
		const WindowPacket::QueryPointer packet {};
		send_packet(packet, __FUNCTION__);
	}

	void Window::query_keymap()
	{
		const WindowPacket::QueryKeymap packet {};
		send_packet(packet, __FUNCTION__);
	}

	void Window::on_socket_error(BAN::StringView function)
	{
		if (m_handling_socket_error)
			return;
		m_handling_socket_error = true;

		dprintln("Socket error while running Window::{}", function);

		if (!m_socket_error_callback)
			exit(1);

		m_socket_error_callback();
		cleanup();
	}

	void Window::cleanup()
	{
		shmdt(m_framebuffer_shm);
		close(m_server_fd);
		close(m_epoll_fd);
	}

	BAN::ErrorOr<bool> Window::handle_resize_event(const EventPacket::ResizeWindowEvent& event)
	{
		void* framebuffer_addr = shmat(event.shmid, nullptr, 0);
		if (framebuffer_addr == SHM_FAILED)
		{
			if (errno == ENOENT)
				return false;
			return BAN::Error::from_errno(errno);
		}

		if (m_framebuffer_shm)
			shmdt(m_framebuffer_shm);
		m_framebuffer_shm = nullptr;

		TRY(m_texture.resize(event.width, event.height));

		if (m_root_widget)
			TRY(m_root_widget->set_fixed_geometry({ 0, 0, event.width, event.height }));

		m_framebuffer_shm = static_cast<uint32_t*>(framebuffer_addr);
		m_width = event.width;
		m_height = event.height;

		invalidate();

		return true;
	}

	void Window::wait_events(const timespec* timeout)
	{
		epoll_event dummy;
		epoll_pwait2(m_epoll_fd, &dummy, 1, timeout, nullptr);
	}

	void Window::poll_events()
	{
		for (;;)
		{
			epoll_event event;
			if (epoll_wait(m_epoll_fd, &event, 1, 0) == 0)
				break;

			if (event.events & (EPOLLHUP | EPOLLERR))
				return on_socket_error(__FUNCTION__);

			ASSERT(event.events & EPOLLIN);

			{
				const ssize_t nrecv = recv(m_server_fd, m_in_buffer.data() + m_in_buffer_size, m_in_buffer.size() - m_in_buffer_size, 0);
				if (nrecv <= 0)
					return on_socket_error(__FUNCTION__);
				if (nrecv > 0)
					m_in_buffer_size += nrecv;
			}

			BAN::ConstByteSpan in_span = m_in_buffer.span().slice(0, m_in_buffer_size);
			while (in_span.size() >= sizeof(PacketHeader))
			{
				const auto header = in_span.as<const PacketHeader>();
				if (in_span.size() < header.size || header.size < sizeof(LibGUI::PacketHeader))
					break;

				const auto packet_span = in_span.slice(0, header.size);

				if (m_initializing)
				{
					if (header.type == PacketType::ResizeWindowEvent)
					{
						if (auto event_or_error = EventPacket::ResizeWindowEvent::deserialize(packet_span); event_or_error.is_error())
							m_init_failed = true;
						else if (handle_resize_event(event_or_error.release_value()).is_error())
							m_init_failed = true;
						else if (m_temp_buffer.size() + m_in_buffer_size > m_in_buffer.size())
							m_init_failed = true;
						else
						{
							memmove(m_in_buffer.data() + m_temp_buffer.size(), m_in_buffer.data(), m_in_buffer.size());
							memcpy(m_in_buffer.data(), m_temp_buffer.data(), m_temp_buffer.size());
							m_in_buffer_size += m_temp_buffer.size();
							m_temp_buffer.clear();
							m_initializing = false;
						}
						return;
					}
					else if (header.type == PacketType::DestroyWindowEvent || header.type == PacketType::CloseWindowEvent)
					{
						m_init_failed = true;
						return;
					}
					else
					{
						const size_t old_size = m_temp_buffer.size();
						MUST(m_temp_buffer.resize(old_size + packet_span.size()));
						memcpy(m_temp_buffer.data() + old_size, packet_span.data(), packet_span.size());
					}
				}
				else switch (header.type)
				{
#define TRY_OR_BREAK(...) ({ auto&& e = (__VA_ARGS__); if (e.is_error()) break; e.release_value(); })
					case PacketType::DestroyWindowEvent:
						exit(1);
					case PacketType::CloseWindowEvent:
						if (m_close_window_event_callback)
							m_close_window_event_callback();
						else
							exit(0);
						break;
					case PacketType::ResizeWindowEvent:
						if (auto result = handle_resize_event(TRY_OR_BREAK(EventPacket::ResizeWindowEvent::deserialize(packet_span))); result.is_error())
							return on_socket_error("handle_resize_event");
						else if (result.value() && m_resize_window_event_callback)
							m_resize_window_event_callback();
						break;
					case PacketType::WindowShownEvent:
						if (m_window_shown_event_callback)
							m_window_shown_event_callback(TRY_OR_BREAK(EventPacket::WindowShownEvent::deserialize(packet_span)).event);
						break;
					case PacketType::WindowFocusEvent:
						if (m_window_focus_event_callback)
							m_window_focus_event_callback(TRY_OR_BREAK(EventPacket::WindowFocusEvent::deserialize(packet_span)).event);
						break;
					case PacketType::WindowFullscreenEvent:
						if (m_window_fullscreen_event_callback)
							m_window_fullscreen_event_callback(TRY_OR_BREAK(EventPacket::WindowFullscreenEvent::deserialize(packet_span)).event);
						break;
					case PacketType::WindowMoveEvent:
						if (m_window_move_event_callback)
							m_window_move_event_callback(TRY_OR_BREAK(EventPacket::WindowMoveEvent::deserialize(packet_span)).event);
						break;
					case PacketType::KeyEvent:
						if (m_key_event_callback)
							m_key_event_callback(TRY_OR_BREAK(EventPacket::KeyEvent::deserialize(packet_span)).event);
						break;
					case PacketType::MouseButtonEvent:
					{
						auto event = TRY_OR_BREAK(EventPacket::MouseButtonEvent::deserialize(packet_span)).event;
						if (m_mouse_button_event_callback)
							m_mouse_button_event_callback(event);
						if (m_root_widget)
							m_root_widget->on_mouse_button(event);
						break;
					}
					case PacketType::MouseMoveEvent:
					{
						auto event = TRY_OR_BREAK(EventPacket::MouseMoveEvent::deserialize(packet_span)).event;
						if (m_mouse_move_event_callback)
							m_mouse_move_event_callback(event);
						if (m_root_widget)
						{
							m_root_widget->before_mouse_move();
							m_root_widget->on_mouse_move(event);
							m_root_widget->after_mouse_move();
						}
						break;
					}
					case PacketType::MouseScrollEvent:
						if (m_mouse_scroll_event_callback)
							m_mouse_scroll_event_callback(TRY_OR_BREAK(EventPacket::MouseScrollEvent::deserialize(packet_span)).event);
						break;
					case PacketType::QueryPointerEvent:
						if (m_query_pointer_event_callback)
							m_query_pointer_event_callback(TRY_OR_BREAK(EventPacket::QueryPointerEvent::deserialize(packet_span)).event);
						break;
					case PacketType::QueryKeymapEvent:
						if (m_query_keymap_event_callback)
							m_query_keymap_event_callback(TRY_OR_BREAK(EventPacket::QueryKeymapEvent::deserialize(packet_span)).event);
						break;
#undef TRY_OR_BREAK
					default:
						dprintln("unhandled packet type: {}", static_cast<uint32_t>(header.type));
						break;
				}

				in_span = in_span.slice(header.size);
			}

			// NOTE: this will only move a single partial packet, so this is fine
			memmove(
				m_in_buffer.data(),
				in_span.data(),
				in_span.size()
			);
			m_in_buffer_size = in_span.size();

			if (m_in_buffer_size >= sizeof(LibGUI::PacketHeader))
			{
				const auto header = BAN::ConstByteSpan(m_in_buffer.span()).as<const LibGUI::PacketHeader>();
				if (header.size < sizeof(LibGUI::PacketHeader) || header.size > m_in_buffer.size())
				{
					dwarnln("server tried to send a {} byte packet", header.size);
					return on_socket_error(__FUNCTION__);
				}
			}
		}

		if (m_root_widget)
		{
			const auto invalidated = m_root_widget->render(m_texture, { 0, 0 }, { 0, 0, m_width, m_height });
			if (invalidated.w && invalidated.h)
				invalidate(invalidated.x, invalidated.y, invalidated.w, invalidated.h);
		}
	}

}
