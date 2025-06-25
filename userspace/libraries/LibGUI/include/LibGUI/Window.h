#pragma once

#include <BAN/Function.h>
#include <BAN/StringView.h>
#include <BAN/UniqPtr.h>

#include <LibGUI/Packet.h>
#include <LibGUI/Texture.h>

namespace LibFont { class Font; }

namespace LibGUI
{

	class Window
	{
	public:
		using Attributes = WindowPacket::Attributes;

		static constexpr Attributes default_attributes = {
			.title_bar       = true,
			.movable         = true,
			.focusable       = true,
			.rounded_corners = true,
			.alpha_channel   = false,
			.resizable       = false,
			.shown           = true,
		};

	public:
		~Window();

		static BAN::ErrorOr<BAN::UniqPtr<Window>> create(uint32_t width, uint32_t height, BAN::StringView title, Attributes attributes = default_attributes);

		Texture& texture() { return m_texture; }
		const Texture& texture() const { return m_texture; }

		void invalidate(int32_t x, int32_t y, uint32_t width, uint32_t height);
		void invalidate() { return invalidate(0, 0, width(), height()); }

		void set_mouse_capture(bool captured);
		void set_fullscreen(bool fullscreen);
		void set_title(BAN::StringView title);

		void set_position(int32_t x, int32_t y);

		Attributes get_attributes() const { return m_attributes; }
		void set_attributes(Attributes attributes);

		void set_min_size(uint32_t width, uint32_t height);
		void set_max_size(uint32_t width, uint32_t height);

		// send resize request to window server
		// actual resize is only done after resize callback is called
		void request_resize(uint32_t width, uint32_t height);

		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }

		void wait_events();
		void poll_events();

		void set_socket_error_callback(BAN::Function<void()> callback)                                             { m_socket_error_callback = callback; }
		void set_close_window_event_callback(BAN::Function<void()> callback)                                       { m_close_window_event_callback = callback; }
		void set_resize_window_event_callback(BAN::Function<void()> callback)                                      { m_resize_window_event_callback = callback; }
		void set_key_event_callback(BAN::Function<void(EventPacket::KeyEvent::event_t)> callback)                  { m_key_event_callback = callback; }
		void set_mouse_button_event_callback(BAN::Function<void(EventPacket::MouseButtonEvent::event_t)> callback) { m_mouse_button_event_callback = callback; }
		void set_mouse_move_event_callback(BAN::Function<void(EventPacket::MouseMoveEvent::event_t)> callback)     { m_mouse_move_event_callback = callback; }
		void set_mouse_scroll_event_callback(BAN::Function<void(EventPacket::MouseScrollEvent::event_t)> callback) { m_mouse_scroll_event_callback = callback; }
		void set_window_shown_event_callback(BAN::Function<void(EventPacket::WindowShownEvent::event_t)> callback) { m_window_shown_event_callback = callback; }

		int server_fd() const { return m_server_fd; }

	private:
		Window(int server_fd, Attributes attributes)
			: m_server_fd(server_fd)
			, m_attributes(attributes)
		{ }

		void on_socket_error(BAN::StringView function);
		void cleanup();

		BAN::ErrorOr<void> handle_resize_event(const EventPacket::ResizeWindowEvent&);

	private:
		const int m_server_fd;

		bool m_handling_socket_error { false };

		Attributes m_attributes;

		uint32_t* m_framebuffer_smo { nullptr };
		uint32_t m_width { 0 };
		uint32_t m_height { 0 };

		Texture m_texture;

		BAN::Function<void()>                                       m_socket_error_callback;
		BAN::Function<void()>                                       m_close_window_event_callback;
		BAN::Function<void()>                                       m_resize_window_event_callback;
		BAN::Function<void(EventPacket::WindowShownEvent::event_t)> m_window_shown_event_callback;
		BAN::Function<void(EventPacket::KeyEvent::event_t)>         m_key_event_callback;
		BAN::Function<void(EventPacket::MouseButtonEvent::event_t)> m_mouse_button_event_callback;
		BAN::Function<void(EventPacket::MouseMoveEvent::event_t)>   m_mouse_move_event_callback;
		BAN::Function<void(EventPacket::MouseScrollEvent::event_t)> m_mouse_scroll_event_callback;

		friend class BAN::UniqPtr<Window>;
	};

}
