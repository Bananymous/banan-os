#pragma once

#include <LibFont/Font.h>

#include <LibGUI/Window.h>

struct Rectangle
{
	uint32_t x      { 0 };
	uint32_t y      { 0 };
	uint32_t width  { 0 };
	uint32_t height { 0 };

	Rectangle get_bounding_box(Rectangle other) const
	{
		const auto min_x = BAN::Math::min(x, other.x);
		const auto min_y = BAN::Math::min(y, other.y);
		const auto max_x = BAN::Math::max(x + width, other.x + other.width);
		const auto max_y = BAN::Math::max(y + height, other.y + other.height);
		return Rectangle {
			.x = min_x,
			.y = min_y,
			.width = max_x - min_x,
			.height = max_y - min_y,
		};
	}
};

class Terminal
{
public:
	void run();

	uint32_t cols() const { return m_window->width() / m_font.width(); }
	uint32_t rows() const { return m_window->height() / m_font.height(); }

private:
	void handle_sgr();
	Rectangle handle_csi(char ch);
	Rectangle putchar(uint8_t ch);
	bool read_shell();

	void hide_cursor();
	void show_cursor();

	void on_key_event(LibGUI::EventPacket::KeyEvent::event_t);

	void start_shell();

private:
	struct Cursor
	{
		uint32_t x;
		uint32_t y;
	};

	struct ShellInfo
	{
		int pts_master;
		pid_t pid;
	};

	enum class State
	{
		Normal,
		ESC,
		CSI,
	};

	struct CSIInfo
	{
		int32_t fields[2];
		size_t index;
		bool question;
	};

private:
	BAN::UniqPtr<LibGUI::Window> m_window;
	LibFont::Font m_font;
	ShellInfo m_shell_info;
	State m_state { State::Normal };
	CSIInfo m_csi_info;

	bool m_cursor_shown { true };
	bool m_cursor_blink_shown { true };
	uint64_t m_cursor_blink_ms { 0 };
	Cursor m_cursor { 0, 0 };
	BAN::Vector<uint32_t> m_cursor_buffer;
	bool m_got_key_event { false };

	uint8_t m_utf8_index { 0 };
	uint8_t m_utf8_bytes[4] { };

	Cursor m_saved_cursor { 0, 0 };
	uint32_t m_fg_color { 0 };
	uint32_t m_bg_color { 0 };
};
