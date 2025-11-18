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
	void handle_sgr(int32_t value);
	Rectangle scroll(uint32_t scroll);
	Rectangle handle_csi(char ch);
	Rectangle putcodepoint(uint32_t codepoint);
	Rectangle putchar(uint8_t ch);
	bool read_shell();

	void update_selection(bool show = true);

	BAN::Optional<uint32_t> get_8bit_color();
	BAN::Optional<uint32_t> get_24bit_color();

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
		static constexpr size_t max_fields = 5;
		int32_t fields[max_fields];
		size_t index;
		bool question;
	};

	struct Cell
	{
		uint32_t codepoint;
		uint32_t fg_color;
		uint32_t bg_color;
		bool bold;
	};

private:
	BAN::UniqPtr<LibGUI::Window> m_window;
	LibFont::Font m_font;
	ShellInfo m_shell_info;
	State m_state { State::Normal };
	CSIInfo m_csi_info;

	BAN::Vector<Cell> m_cells;
	uint32_t m_cells_rows { 0 };
	uint32_t m_cells_cols { 0 };

	uint32_t m_selection_s_col { UINT32_MAX };
	uint32_t m_selection_s_row { UINT32_MAX };
	uint32_t m_selection_e_col { UINT32_MAX };
	uint32_t m_selection_e_row { UINT32_MAX };
	bool m_selecting { false };
	bool m_brackted_paste_mode { false };

	bool m_cursor_shown { true };
	bool m_cursor_blink_shown { true };
	uint64_t m_cursor_blink_ms { 0 };
	Cursor m_cursor { 0, 0 };
	BAN::Vector<uint32_t> m_cursor_buffer;
	bool m_got_key_event { false };

	uint8_t m_utf8_index { 0 };
	uint8_t m_utf8_bytes[4] { };

	uint32_t m_last_graphic_char { 0 };

	Cursor m_saved_cursor { 0, 0 };
	uint32_t m_fg_color { 0 };
	uint32_t m_bg_color { 0 };
	bool m_colors_inverted { false };
	bool m_is_bold { false };
};
