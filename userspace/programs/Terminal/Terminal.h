#pragma once

#include <LibFont/Font.h>

#include <LibGUI/Window.h>

class Terminal
{
public:
	void run();

	uint32_t cols() const { return m_window->width() / m_font.width(); }
	uint32_t rows() const { return m_window->height() / m_font.height(); }

private:
	void handle_csi(char ch);
	void handle_sgr();
	void putchar(uint8_t ch);
	bool read_shell(int fd);

	void on_key_event(LibGUI::EventPacket::KeyEvent);

	void start_shell();

private:
	struct Cursor
	{
		uint32_t x;
		uint32_t y;
	};

	struct ShellInfo
	{
		int in;
		int out;
		int err;
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
	};

private:
	BAN::UniqPtr<LibGUI::Window> m_window;
	LibFont::Font m_font;
	Cursor m_cursor { 0, 0 };
	ShellInfo m_shell_info;
	State m_state { State::Normal };
	CSIInfo m_csi_info;

	uint8_t m_utf8_index { 0 };
	uint8_t m_utf8_bytes[4] { };

	Cursor m_saved_cursor { 0, 0 };
	uint32_t m_fg_color { 0xFFFFFF };
	uint32_t m_bg_color { 0x000000 };
};
