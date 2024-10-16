#pragma once

#include <BAN/Function.h>
#include <BAN/StringView.h>
#include <BAN/UniqPtr.h>

#include <LibGUI/Packet.h>

namespace LibFont { class Font; }

namespace LibGUI
{

	class Window
	{
	public:
		struct Attributes
		{
			bool title_bar       { true };
			bool movable         { true };
			bool rounded_corners { true };
			bool alpha_channel   { false };
		};

	public:
		~Window();

		static BAN::ErrorOr<BAN::UniqPtr<Window>> create(uint32_t width, uint32_t height, BAN::StringView title);

		void set_pixel(uint32_t x, uint32_t y, uint32_t color)
		{
			ASSERT(x < m_width);
			ASSERT(y < m_height);
			m_framebuffer[y * m_width + x] = color;
		}

		uint32_t get_pixel(uint32_t x, uint32_t y) const
		{
			ASSERT(x < m_width);
			ASSERT(y < m_height);
			return m_framebuffer[y * m_width + x];
		}

		void fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
		void fill(uint32_t color) { return fill_rect(0, 0, width(), height(), color); }

		void draw_character(uint32_t codepoint, const LibFont::Font& font, int32_t x, int32_t y, uint32_t color);
		void draw_text(BAN::StringView text, const LibFont::Font& font, int32_t x, int32_t y, uint32_t color);

		// shift whole vertically by amount pixels, sign determines the direction
		// fill_color is used to fill "new" data
		void shift_vertical(int32_t amount, uint32_t fill_color);

		// copy horizontal slice [src_y, src_y + amount[ to [dst_y, dst_y + amount[
		// fill_color is used when copying data outside of window bounds
		void copy_horizontal_slice(int32_t dst_y, int32_t src_y, uint32_t amount, uint32_t fill_color);

		bool invalidate(int32_t x, int32_t y, uint32_t width, uint32_t height);
		bool invalidate() { return invalidate(0, 0, width(), height()); }

		bool set_position(int32_t x, int32_t y);

		Attributes get_attributes() const { return m_attributes; }
		bool set_attributes(Attributes attributes);

		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }

		void poll_events();
		void set_close_window_event_callback(BAN::Function<void()> callback)                                       { m_close_window_event_callback = callback; }
		void set_key_event_callback(BAN::Function<void(EventPacket::KeyEvent::event_t)> callback)                  { m_key_event_callback = callback; }
		void set_mouse_button_event_callback(BAN::Function<void(EventPacket::MouseButtonEvent::event_t)> callback) { m_mouse_button_event_callback = callback; }
		void set_mouse_move_event_callback(BAN::Function<void(EventPacket::MouseMoveEvent::event_t)> callback)     { m_mouse_move_event_callback = callback; }
		void set_mouse_scroll_event_callback(BAN::Function<void(EventPacket::MouseScrollEvent::event_t)> callback) { m_mouse_scroll_event_callback = callback; }

		int server_fd() const { return m_server_fd; }

	private:
		Window(int server_fd, uint32_t* framebuffer_smo, BAN::Vector<uint32_t>&& framebuffer, uint32_t width, uint32_t height)
			: m_server_fd(server_fd)
			, m_framebuffer(framebuffer)
			, m_framebuffer_smo(framebuffer_smo)
			, m_width(width)
			, m_height(height)
		{ }

		bool clamp_to_framebuffer(int32_t& x, int32_t& y, uint32_t& width, uint32_t& height) const;

	private:
		int m_server_fd;

		Attributes m_attributes;

		BAN::Vector<uint32_t> m_framebuffer;
		uint32_t* m_framebuffer_smo;
		uint32_t m_width;
		uint32_t m_height;

		BAN::Function<void()>                                       m_close_window_event_callback;
		BAN::Function<void(EventPacket::KeyEvent::event_t)>         m_key_event_callback;
		BAN::Function<void(EventPacket::MouseButtonEvent::event_t)> m_mouse_button_event_callback;
		BAN::Function<void(EventPacket::MouseMoveEvent::event_t)>   m_mouse_move_event_callback;
		BAN::Function<void(EventPacket::MouseScrollEvent::event_t)> m_mouse_scroll_event_callback;

		friend class BAN::UniqPtr<Window>;
	};

}
