#include "LibGUI/Window.h"

#include <BAN/ScopeGuard.h>

#include <LibFont/Font.h>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/banan-os.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace LibGUI
{

	struct ReceivePacket
	{
		PacketType type;
		BAN::Vector<uint8_t> data_with_type;
	};

	static BAN::ErrorOr<ReceivePacket> recv_packet(int socket)
	{
		uint32_t packet_size;

		{
			const ssize_t nrecv = recv(socket, &packet_size, sizeof(uint32_t), 0);
			if (nrecv < 0)
				return BAN::Error::from_errno(errno);
			if (nrecv == 0)
				return BAN::Error::from_errno(ECONNRESET);
		}

		if (packet_size < sizeof(uint32_t))
			return BAN::Error::from_literal("invalid packet, does not fit packet id");

		BAN::Vector<uint8_t> packet_data;
		TRY(packet_data.resize(packet_size));

		size_t total_recv = 0;
		while (total_recv < packet_size)
		{
			const ssize_t nrecv = recv(socket, packet_data.data() + total_recv, packet_size - total_recv, 0);
			if (nrecv < 0)
				return BAN::Error::from_errno(errno);
			if (nrecv == 0)
				return BAN::Error::from_errno(ECONNRESET);
			total_recv += nrecv;
		}

		return ReceivePacket {
			*reinterpret_cast<PacketType*>(packet_data.data()),
			packet_data
		};
	}

	Window::~Window()
	{
		cleanup();
	}

	BAN::ErrorOr<BAN::UniqPtr<Window>> Window::create(uint32_t width, uint32_t height, BAN::StringView title, Attributes attributes)
	{
		int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (server_fd == -1)
			return BAN::Error::from_errno(errno);
		BAN::ScopeGuard server_closer([server_fd] { close(server_fd); });

		if (fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL) | O_CLOEXEC) == -1)
			return BAN::Error::from_errno(errno);

		timespec start_time;
		clock_gettime(CLOCK_MONOTONIC, &start_time);

		for (;;)
		{
			sockaddr_un server_address;
			server_address.sun_family = AF_UNIX;
			strcpy(server_address.sun_path, s_window_server_socket.data());
			if (connect(server_fd, (sockaddr*)&server_address, sizeof(server_address)) == 0)
				break;

			timespec current_time;
			clock_gettime(CLOCK_MONOTONIC, &current_time);
			time_t duration_s = (current_time.tv_sec - start_time.tv_sec) + (current_time.tv_nsec >= start_time.tv_nsec);
			if (duration_s > 1)
				return BAN::Error::from_errno(ETIMEDOUT);

			timespec sleep_time;
			sleep_time.tv_sec = 0;
			sleep_time.tv_nsec = 1'000'000;
			nanosleep(&sleep_time, nullptr);
		}

		WindowPacket::WindowCreate create_packet;
		create_packet.width = width;
		create_packet.height = height;
		create_packet.attributes = attributes;
		TRY(create_packet.title.append(title));
		TRY(create_packet.send_serialized(server_fd));

		auto window = TRY(BAN::UniqPtr<Window>::create(server_fd, attributes));

		bool resized = false;
		window->set_resize_window_event_callback([&]() { resized = true; });
		while (!resized)
			window->poll_events();
		window->set_resize_window_event_callback({});

		server_closer.disable();

		return window;
	}

	void Window::fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color)
	{
		if (!clamp_to_framebuffer(x, y, width, height))
			return;
		for (uint32_t y_off = 0; y_off < height; y_off++)
			for (uint32_t x_off = 0; x_off < width; x_off++)
				set_pixel(x + x_off, y + y_off, color);
	}

	void Window::draw_character(uint32_t codepoint, const LibFont::Font& font, int32_t tl_x, int32_t tl_y, uint32_t color)
	{
		if (tl_y + (int32_t)font.height() < 0 || tl_y >= (int32_t)height())
			return;
		if (tl_x + (int32_t)font.width() < 0 || tl_x >= (int32_t)width())
			return;

		auto glyph = font.glyph(codepoint);
		if (glyph == nullptr)
			return;

		for (int32_t off_y = 0; off_y < (int32_t)font.height(); off_y++)
		{
			if (tl_y + off_y < 0)
				continue;
			uint32_t abs_y = tl_y + off_y;
			if (abs_y >= height())
				break;
			for (int32_t off_x = 0; off_x < (int32_t)font.width(); off_x++)
			{
				if (tl_x + off_x < 0)
					continue;
				uint32_t abs_x = tl_x + off_x;
				if (abs_x >= width())
					break;
				const uint8_t bitmask = 1 << (font.width() - off_x - 1);
				if (glyph[off_y * font.pitch()] & bitmask)
					set_pixel(abs_x, abs_y, color);
			}
		}
	}

	void Window::draw_text(BAN::StringView text, const LibFont::Font& font, int32_t tl_x, int32_t tl_y, uint32_t color)
	{
		for (size_t i = 0; i < text.size(); i++)
			draw_character(text[i], font, tl_x + (int32_t)(i * font.width()), tl_y, color);
	}

	void Window::shift_vertical(int32_t amount, uint32_t fill_color)
	{
		const uint32_t amount_abs = BAN::Math::abs(amount);
		if (amount_abs == 0 || amount_abs >= height())
			return;

		uint32_t* dst = (amount > 0) ? m_framebuffer.data() + width() * amount_abs : m_framebuffer.data();
		uint32_t* src = (amount < 0) ? m_framebuffer.data() + width() * amount_abs : m_framebuffer.data();
		memmove(dst, src, width() * (height() - amount_abs) * 4);

		const uint32_t y_lo = (amount < 0) ? height() - amount_abs : 0;
		const uint32_t y_hi = (amount < 0) ? height()              : amount_abs;
		for (uint32_t y = y_lo; y < y_hi; y++)
			for (uint32_t x = 0; x < width(); x++)
				set_pixel(x, y, fill_color);
	}

	void Window::copy_horizontal_slice(int32_t dst_y, int32_t src_y, uint32_t uamount, uint32_t fill_color)
	{
		int32_t amount = uamount;
		if (dst_y < 0)
		{
			amount -= -dst_y;
			src_y  += -dst_y;
			dst_y   =  0;
		}

		amount = BAN::Math::min<int32_t>(amount, height() - dst_y);
		if (amount <= 0)
			return;

		const int32_t copy_src_y  = BAN::Math::clamp<int32_t>(src_y, 0, height());
		const int32_t copy_amount = BAN::Math::clamp<int32_t>(src_y + amount, 0, height()) - copy_src_y;
		if (copy_amount > 0)
		{
			memmove(
				&m_framebuffer[width() * (dst_y + (copy_src_y - src_y))],
				&m_framebuffer[width() * copy_src_y],
				copy_amount * width() * 4
			);
		}

		const uint32_t fill_y_off = (src_y < copy_src_y) ? 0 : copy_amount;
		const uint32_t fill_amount = amount - copy_amount;
		for (uint32_t i = 0; i < fill_amount; i++)
			for (uint32_t x = 0; x < width(); x++)
				set_pixel(x, dst_y + fill_y_off + i, fill_color);
	}

	bool Window::clamp_to_framebuffer(int32_t& signed_x, int32_t& signed_y, uint32_t& width, uint32_t& height) const
	{
		int32_t min_x = BAN::Math::max<int32_t>(signed_x, 0);
		int32_t min_y = BAN::Math::max<int32_t>(signed_y, 0);
		int32_t max_x = BAN::Math::min<int32_t>(this->width(), signed_x + (int32_t)width);
		int32_t max_y = BAN::Math::min<int32_t>(this->height(), signed_y + (int32_t)height);

		if (min_x >= max_x)
			return false;
		if (min_y >= max_y)
			return false;

		signed_x = min_x;
		signed_y = min_y;
		width = max_x - min_x;
		height = max_y - min_y;
		return true;
	}

	void Window::invalidate(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		if (!clamp_to_framebuffer(x, y, width, height))
			return;

		for (uint32_t i = 0; i < height; i++)
			memcpy(&m_framebuffer_smo[(y + i) * m_width + x], &m_framebuffer[(y + i) * m_width + x], width * sizeof(uint32_t));

		WindowPacket::WindowInvalidate packet;
		packet.x = x;
		packet.y = y;
		packet.width = width;
		packet.height = height;

		if (auto ret = packet.send_serialized(m_server_fd); ret.is_error())
			return on_socket_error(__FUNCTION__);
	}

	void Window::set_mouse_capture(bool captured)
	{
		WindowPacket::WindowSetMouseCapture packet;
		packet.captured = captured;

		if (auto ret = packet.send_serialized(m_server_fd); ret.is_error())
			return on_socket_error(__FUNCTION__);
	}

	void Window::set_fullscreen(bool fullscreen)
	{
		WindowPacket::WindowSetFullscreen packet;
		packet.fullscreen = fullscreen;

		if (auto ret = packet.send_serialized(m_server_fd); ret.is_error())
			return on_socket_error(__FUNCTION__);
	}

	void Window::set_position(int32_t x, int32_t y)
	{
		WindowPacket::WindowSetPosition packet;
		packet.x = x;
		packet.y = y;

		if (auto ret = packet.send_serialized(m_server_fd); ret.is_error())
			return on_socket_error(__FUNCTION__);
	}

	void Window::set_attributes(Attributes attributes)
	{
		WindowPacket::WindowSetAttributes packet;
		packet.attributes = attributes;

		if (auto ret = packet.send_serialized(m_server_fd); ret.is_error())
			return on_socket_error(__FUNCTION__);

		m_attributes = attributes;
	}

	void Window::request_resize(uint32_t width, uint32_t height)
	{
		WindowPacket::WindowSetSize packet;
		packet.width = width;
		packet.height = height;

		if (auto ret = packet.send_serialized(m_server_fd); ret.is_error())
			return on_socket_error(__FUNCTION__);
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
		munmap(m_framebuffer_smo, m_width * m_height * 4);
		close(m_server_fd);
	}

	BAN::ErrorOr<void> Window::handle_resize_event(const EventPacket::ResizeWindowEvent& event)
	{
		if (m_framebuffer_smo)
			munmap(m_framebuffer_smo, m_width * m_height * 4);
		m_framebuffer_smo = nullptr;

		BAN::Vector<uint32_t> framebuffer;
		TRY(framebuffer.resize(event.width * event.height, 0xFFFFFFFF));

		void* framebuffer_addr = smo_map(event.smo_key);
		if (framebuffer_addr == nullptr)
			return BAN::Error::from_errno(errno);

		const uint32_t min_x = BAN::Math::min(m_width, event.width);
		const uint32_t min_y = BAN::Math::min(m_height, event.height);
		for (uint32_t y = 0; y < min_y; y++)
			for (uint32_t x = 0; x < min_x; x++)
				framebuffer[y * event.width + x] = m_framebuffer[y * m_width + x];

		m_framebuffer_smo = static_cast<uint32_t*>(framebuffer_addr);
		m_framebuffer = BAN::move(framebuffer);
		m_width = event.width;
		m_height = event.height;

		invalidate();

		return {};
	}

#define TRY_OR_BREAK(...) ({ auto&& e = (__VA_ARGS__); if (e.is_error()) break; e.release_value(); })

	void Window::poll_events()
	{
		for (;;)
		{
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(m_server_fd, &fds);
			timeval timeout { .tv_sec = 0, .tv_usec = 0 };
			select(m_server_fd + 1, &fds, nullptr, nullptr, &timeout);

			if (!FD_ISSET(m_server_fd, &fds))
				break;

			auto packet_or_error = recv_packet(m_server_fd);
			if (packet_or_error.is_error())
				return on_socket_error(__FUNCTION__);

			const auto [packet_type, packet_data] = packet_or_error.release_value();
			switch (packet_type)
			{
				case PacketType::DestroyWindowEvent:
					exit(1);
				case PacketType::CloseWindowEvent:
					if (m_close_window_event_callback)
						m_close_window_event_callback();
					else
						exit(0);
					break;
				case PacketType::ResizeWindowEvent:
				{
					MUST(handle_resize_event(TRY_OR_BREAK(EventPacket::ResizeWindowEvent::deserialize(packet_data.span()))));
					if (m_resize_window_event_callback)
						m_resize_window_event_callback();
					break;
				}
				case PacketType::KeyEvent:
					if (m_key_event_callback)
						m_key_event_callback(TRY_OR_BREAK(EventPacket::KeyEvent::deserialize(packet_data.span())).event);
					break;
				case PacketType::MouseButtonEvent:
					if (m_mouse_button_event_callback)
						m_mouse_button_event_callback(TRY_OR_BREAK(EventPacket::MouseButtonEvent::deserialize(packet_data.span())).event);
					break;
				case PacketType::MouseMoveEvent:
					if (m_mouse_move_event_callback)
						m_mouse_move_event_callback(TRY_OR_BREAK(EventPacket::MouseMoveEvent::deserialize(packet_data.span())).event);
					break;
				case PacketType::MouseScrollEvent:
					if (m_mouse_scroll_event_callback)
						m_mouse_scroll_event_callback(TRY_OR_BREAK(EventPacket::MouseScrollEvent::deserialize(packet_data.span())).event);
					break;
				default:
					break;
			}
		}
	}

}
