#pragma once

#include "Utils.h"

#include <BAN/RefPtr.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <LibFont/Font.h>
#include <LibGUI/Window.h>

class Window : public BAN::RefCounted<Window>
{
public:
	Window(int fd, const LibFont::Font& font)
		: m_font(font)
		, m_client_fd(fd)
	{ }

	~Window();

	void set_position(Position position)
	{
		m_client_area.x = position.x;
		m_client_area.y = position.y;
	}

	int client_fd() const { return m_client_fd; }
	long smo_key() const { return m_smo_key; }

	int32_t client_x() const { return m_client_area.x; }
	int32_t client_y() const { return m_client_area.y; }
	int32_t client_width() const { return m_client_area.width; }
	int32_t client_height() const { return m_client_area.height; }
	Rectangle client_size() const { return { 0, 0, client_width(), client_height() }; }
	Rectangle client_area() const { return m_client_area; }

	int32_t title_bar_x() const { return client_x(); }
	int32_t title_bar_y() const { return m_attributes.title_bar ? client_y() - title_bar_height() : client_y(); }
	int32_t title_bar_width() const { return client_width(); }
	int32_t title_bar_height() const { return m_attributes.title_bar ? m_title_bar_height : 0; }
	Rectangle title_bar_size() const { return { 0, 0, title_bar_width(), title_bar_height() }; }
	Rectangle title_bar_area() const { return { title_bar_x(), title_bar_y(), title_bar_width(), title_bar_height() }; }

	int32_t full_x() const { return title_bar_x(); }
	int32_t full_y() const { return title_bar_y(); }
	int32_t full_width() const { return client_width(); }
	int32_t full_height() const { return client_height() + title_bar_height(); }
	Rectangle full_size() const { return { 0, 0, full_width(), full_height() }; }
	Rectangle full_area() const { return { full_x(), full_y(), full_width(), full_height() }; }

	LibGUI::Window::Attributes get_attributes() const { return m_attributes; };
	void set_attributes(LibGUI::Window::Attributes attributes) { m_attributes = attributes; };

	const uint32_t* framebuffer() const { return m_fb_addr; }

	uint32_t title_bar_pixel(int32_t abs_x, int32_t abs_y, Position cursor) const
	{
		ASSERT(title_bar_area().contains({ abs_x, abs_y }));

		if (auto close_button = close_button_area(); close_button.contains({ abs_x, abs_y }))
			return close_button.contains(cursor) ? 0xFFFF0000 : 0xFFD00000;

		int32_t rel_x = abs_x - title_bar_x();
		int32_t rel_y = abs_y - title_bar_y();
		return m_title_bar_data[rel_y * title_bar_width() + rel_x];
	}

	Circle close_button_area() const { return { title_bar_x() + title_bar_width() - title_bar_height() / 2, title_bar_y() + title_bar_height() / 2, title_bar_height() * 3 / 8 }; }
	Rectangle title_text_area() const { return { title_bar_x(), title_bar_y(), title_bar_width() - title_bar_height(), title_bar_height() }; }

	BAN::ErrorOr<void> initialize(BAN::StringView title, uint32_t width, uint32_t height);
	BAN::ErrorOr<void> resize(uint32_t width, uint32_t height);

private:
	BAN::ErrorOr<void> prepare_title_bar();

private:
	static constexpr int32_t m_title_bar_height { 20 };

	const LibFont::Font& m_font;

	const int   m_client_fd      { -1 };
	Rectangle   m_client_area    { 0, 0, 0, 0 };
	long        m_smo_key        { 0 };
	uint32_t*   m_fb_addr        { nullptr };
	BAN::String m_title;

	BAN::Vector<uint32_t> m_title_bar_data;

	LibGUI::Window::Attributes m_attributes { LibGUI::Window::default_attributes };

	friend class BAN::RefPtr<Window>;
};
