#pragma once

#include <LibFont/Font.h>

#include <LibGUI/Window.h>

#include <BAN/CircularQueue.h>

struct Rectangle
{
	uint32_t x { 0 };
	uint32_t y { 0 };
	uint32_t w { 0 };
	uint32_t h { 0 };

	Rectangle get_bounding_box(Rectangle other) const
	{
		if (this->w == 0 || this->h == 0)
			return other;
		if (other.w == 0 || other.h == 0)
			return *this;
		const auto min_x = BAN::Math::min(x, other.x);
		const auto min_y = BAN::Math::min(y, other.y);
		const auto max_x = BAN::Math::max(x + w, other.x + other.w);
		const auto max_y = BAN::Math::max(y + h, other.y + other.h);
		return Rectangle {
			.x = min_x,
			.y = min_y,
			.w = max_x - min_x,
			.h = max_y - min_y,
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
	struct Cell;

	void handle_sgr(int32_t value);
	void handle_scroll();
	Rectangle handle_csi(char ch);
	Rectangle putcodepoint(uint32_t codepoint);
	Rectangle putchar(uint8_t ch);

	Rectangle read_shell();
	void write_shell(BAN::StringView data);

	void update_selection();

	BAN::Optional<uint32_t> get_8bit_color();
	BAN::Optional<uint32_t> get_24bit_color();

	Rectangle hide_cursor();
	Rectangle show_cursor();

	void on_key_event(LibGUI::EventPacket::KeyEvent::event_t);

	void start_shell();

	void resize();

	Rectangle render_cell(uint32_t x, uint32_t y, const Cell&);
	Rectangle render_visible_area(bool skip_scrollback = false);

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
		char param;
	};

	struct Cell
	{
		uint32_t codepoint;
		uint32_t fg_color;
		uint32_t bg_color;
		bool bold;
	};

	struct Selection
	{
		struct Position
		{
			int32_t x;
			int32_t y;
		};

		Position start;
		Position end;

		int32_t min;
		int32_t max;
	};

	struct ScrollbackLine
	{
		BAN::Vector<Cell> cells;
		bool soft_wrap;
	};
	using Scrollback = BAN::CircularQueue<ScrollbackLine, 1024>;

private:
	BAN::UniqPtr<LibGUI::Window> m_window;
	LibFont::Font m_font;
	ShellInfo m_shell_info;
	State m_state { State::Normal };
	CSIInfo m_csi_info;

	Rectangle m_visible {};

	BAN::Vector<Cell> m_cells;
	uint32_t m_cells_rows { 0 };
	uint32_t m_cells_cols { 0 };

	BAN::Vector<bool> m_soft_wraps;

	BAN::UniqPtr<Scrollback> m_scrollback;
	size_t m_visual_scroll { 0 };

	BAN::Optional<Selection> m_selection;
	bool m_is_selecting { false };
	bool m_brackted_paste_mode { false };

	static constexpr uint64_t m_cursor_blink_interval_ms { 500 };
	uint64_t m_cursor_blink_next_ms { 0 };
	bool m_cursor_blink_toggle { true };
	bool m_cursor_shown { true };
	Cursor m_cursor { 0, 0 };

	uint8_t m_utf8_index { 0 };
	uint8_t m_utf8_bytes[4] { };

	uint32_t m_last_graphic_char { 0 };

	Cursor m_saved_cursor { 0, 0 };
	uint32_t m_fg_color { 0 };
	uint32_t m_bg_color { 0 };
	bool m_colors_inverted { false };
	bool m_is_bold { false };
};
