#include "Terminal.h"

#include <BAN/Debug.h>
#include <BAN/UTF8.h>

#include <ctype.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>

void Terminal::start_shell()
{
	int shell_stdin[2];
	if (pipe(shell_stdin) == -1)
	{
		dwarnln("pipe: {}", strerror(errno));
		exit(1);
	}

	int shell_stdout[2];
	if (pipe(shell_stdout) == -1)
	{
		dwarnln("pipe: {}", strerror(errno));
		exit(1);
	}

	int shell_stderr[2];
	if (pipe(shell_stderr) == -1)
	{
		dwarnln("pipe: {}", strerror(errno));
		exit(1);
	}

	pid_t shell_pid = fork();
	if (shell_pid == 0)
	{
		if (dup2(shell_stdin[0], STDIN_FILENO) == -1)
		{
			dwarnln("dup2: {}", strerror(errno));
			exit(1);
		}
		close(shell_stdin[0]);
		close(shell_stdin[1]);

		if (dup2(shell_stdout[1], STDOUT_FILENO) == -1)
		{
			dwarnln("dup2: {}", strerror(errno));
			exit(1);
		}
		close(shell_stdout[0]);
		close(shell_stdout[1]);

		if (dup2(shell_stderr[1], STDERR_FILENO) == -1)
		{
			dwarnln("dup2: {}", strerror(errno));
			exit(1);
		}
		close(shell_stderr[0]);
		close(shell_stderr[1]);

		execl("/bin/Shell", "Shell", NULL);
		exit(1);
	}

	if (shell_pid == -1)
	{
		dwarnln("fork: {}", strerror(errno));
		exit(1);
	}

	close(shell_stdin[0]);
	close(shell_stdout[1]);
	close(shell_stderr[1]);

	m_shell_info = {
		.in = shell_stdin[1],
		.out = shell_stdout[0],
		.err = shell_stderr[0],
		.pid = shell_pid
	};
}

static volatile bool s_shell_exited = false;

void Terminal::run()
{
	signal(SIGCHLD, [](int) { s_shell_exited = true; });
	start_shell();

	m_window = MUST(LibGUI::Window::create(600, 400, "Terminal"sv));
	m_window->fill(m_bg_color);
	m_window->invalidate();

	m_font = MUST(LibFont::Font::load("/usr/share/fonts/lat0-16.psfu"sv));

	m_window->set_key_event_callback([&](LibGUI::EventPacket::KeyEvent event) { on_key_event(event); });

	const int max_fd = BAN::Math::max(BAN::Math::max(m_shell_info.out, m_shell_info.err), m_window->server_fd());
	while (!s_shell_exited)
	{
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(m_shell_info.out, &fds);
		FD_SET(m_shell_info.err, &fds);
		FD_SET(m_window->server_fd(), &fds);

		select(max_fd + 1, &fds, nullptr, nullptr, nullptr);

		if (FD_ISSET(m_shell_info.out, &fds))
			if (!read_shell(m_shell_info.out))
				break;
		if (FD_ISSET(m_shell_info.err, &fds))
			if (!read_shell(m_shell_info.err))
				break;
		if (FD_ISSET(m_window->server_fd(), &fds))
			m_window->poll_events();
	}
}


bool Terminal::read_shell(int fd)
{
	char buffer[128];
	ssize_t nread = read(fd, buffer, sizeof(buffer) - 1);
	if (nread < 0)
		dwarnln("read: {}", strerror(errno));
	if (nread <= 0)
		return false;
	for (ssize_t i = 0; i < nread; i++)
		putchar(buffer[i]);
	return true;
}

void Terminal::handle_sgr()
{
	constexpr uint32_t colors_default[] {
		0x555555,
		0xFF5555,
		0x55FF55,
		0xFFFF55,
		0x5555FF,
		0xFF55FF,
		0x55FFFF,
		0xFFFFFF,
	};

	constexpr uint32_t colors_bright[] {
		0xAAAAAA,
		0xFFAAAA,
		0xAAFFAA,
		0xFFFFAA,
		0xAAAAFF,
		0xFFAAFF,
		0xAAFFFF,
		0xFFFFFF,
	};

	switch (m_csi_info.fields[0])
	{
		case -1: case 0:
			m_fg_color = 0xFFFFFF;
			m_bg_color = 0x000000;
			break;
		case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
			m_fg_color = colors_default[m_csi_info.fields[0] - 30];
			break;
		case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
			m_bg_color = colors_default[m_csi_info.fields[0] - 40];
			break;
		case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
			m_fg_color = colors_bright[m_csi_info.fields[0] - 90];
			break;
		case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
			m_bg_color = colors_bright[m_csi_info.fields[0] - 100];
			break;
		default:
			dprintln("TODO: SGR {}", m_csi_info.fields[0]);
			break;
	}
}

void Terminal::handle_csi(char ch)
{
	if (ch == ';')
	{
		m_csi_info.index++;
		return;
	}

	if (ch == '?')
		return;

	if (isdigit(ch))
	{
		if (m_csi_info.index <= 1)
		{
			auto& field = m_csi_info.fields[m_csi_info.index];
			field = (BAN::Math::max(field, 0) * 10) + (ch - '0');
		}
		return;
	}

	switch (ch)
	{
		case 'C':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.x = BAN::Math::clamp<int32_t>(m_cursor.x + m_csi_info.fields[0], 0, cols() - 1);
			break;
		case 'D':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.x = BAN::Math::clamp<int32_t>((int32_t)m_cursor.x - m_csi_info.fields[0], 0, cols() - 1);
			break;
		case 'G':
			m_cursor.x = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, cols()) - 1;
			break;
		case 'H':
			m_cursor.y = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, rows()) - 1;
			m_cursor.x = BAN::Math::clamp<int32_t>(m_csi_info.fields[1], 1, cols()) - 1;
			break;
		case 'J':
		{
			uint32_t rects[2][4] { { (uint32_t)-1 }, { (uint32_t)-1 } };

			if (m_csi_info.fields[0] == -1 || m_csi_info.fields[0] == 0)
			{
				rects[0][0] = m_cursor.x * m_font.width();
				rects[0][1] = m_cursor.y * m_font.height();
				rects[0][2] = m_window->width() - rects[0][0];
				rects[0][3] = m_font.height();

				rects[1][0] = 0;
				rects[1][1] = (m_cursor.y + 1) * m_font.height();
				rects[1][2] = m_window->width();
				rects[1][3] = m_window->height() - rects[1][1];
			}
			else if (m_csi_info.fields[0] == 1)
			{
				rects[0][0] = 0;
				rects[0][1] = m_cursor.y * m_font.height();
				rects[0][2] = m_cursor.x * m_font.width();
				rects[0][3] = m_font.height();

				rects[1][0] = 0;
				rects[1][1] = 0;
				rects[1][2] = m_window->width();
				rects[1][3] = m_cursor.y * m_font.height();
			}
			else
			{
				rects[0][0] = 0;
				rects[0][1] = 0;
				rects[0][2] = m_window->width();
				rects[0][3] = m_window->height();
			}

			for (int i = 0; i < 2; i++)
			{
				if (rects[i][0] == (uint32_t)-1)
					continue;
				m_window->fill_rect(rects[i][0], rects[i][1], rects[i][2], rects[i][3], m_bg_color);
				m_window->invalidate(rects[i][0], rects[i][1], rects[i][2], rects[i][3]);
			}
			break;
		}
		case 'K':
		{
			m_csi_info.fields[0] = BAN::Math::max(m_csi_info.fields[0], 0);

			uint32_t rect[4];
			rect[0] = (m_csi_info.fields[0] == 0) ? m_cursor.x * m_font.width() : 0;
			rect[1] = m_cursor.y * m_font.height();
			rect[2] = (m_csi_info.fields[0] == 1) ? m_cursor.x * m_font.width() : m_window->width() - rect[0];
			rect[3] = m_font.height();

			m_window->fill_rect(rect[0], rect[1], rect[2], rect[3], m_bg_color);
			m_window->invalidate(rect[0], rect[1], rect[2], rect[3]);

			break;
		}
		case 'm':
			handle_sgr();
			break;
		case 's':
			m_saved_cursor = m_cursor;
			break;
		case 'u':
			m_cursor = m_saved_cursor;
			break;
		default:
			dprintln("TODO: CSI {}", ch);
			break;
	}
	m_state = State::Normal;
}

void Terminal::putchar(uint32_t codepoint)
{
	if (m_state == State::ESC)
	{
		if (codepoint != '[')
		{
			dprintln("unknown escape character 0x{H}", codepoint);
			m_state = State::Normal;
			return;
		}
		m_state = State::CSI;
		m_csi_info.index = 0;
		m_csi_info.fields[0] = -1;
		m_csi_info.fields[1] = -1;
		return;
	}

	if (m_state == State::CSI)
	{
		if (codepoint < 0x20 || codepoint > 0xFE)
		{
			dprintln("invalid CSI 0x{H}", codepoint);
			m_state = State::Normal;
			return;
		}
		handle_csi(codepoint);
		return;
	}

	switch (codepoint)
	{
		case '\e':
			m_state = State::ESC;
			break;
		case '\n':
			m_cursor.x = 0;
			m_cursor.y++;
			break;
		case '\r':
			m_cursor.x = 0;
			break;
		case '\b':
			if (m_cursor.x > 0)
				m_cursor.x--;
			break;
		default:
		{
			const uint32_t cell_w = m_font.width();
			const uint32_t cell_h = m_font.height();
			const uint32_t cell_x = m_cursor.x * cell_w;
			const uint32_t cell_y = m_cursor.y * cell_h;

			m_window->fill_rect(cell_x, cell_y, cell_w, cell_h, m_bg_color);
			m_window->draw_character(codepoint, m_font, cell_x, cell_y, m_fg_color);
			m_window->invalidate(cell_x, cell_y, cell_w, cell_h);
			m_cursor.x++;
			break;
		}
	}

	if (m_cursor.x >= cols())
	{
		m_cursor.x = 0;
		m_cursor.y++;
	}

	if (m_cursor.y >= rows())
	{
		uint32_t scroll = m_cursor.y - rows() + 1;
		m_cursor.y -= scroll;
		m_window->shift_vertical(-scroll * (int32_t)m_font.height());
		m_window->fill_rect(0, m_window->height() - scroll * m_font.height(), m_window->width(), scroll * m_font.height(), m_bg_color);
		m_window->invalidate();
	}
}

void Terminal::on_key_event(LibGUI::EventPacket::KeyEvent event)
{
	if (event.released())
		return;
	if (const char* text = LibInput::key_to_utf8_ansi(event.key, event.modifier))
		write(m_shell_info.in, text, strlen(text));
}
