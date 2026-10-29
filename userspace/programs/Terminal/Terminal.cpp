#include "Terminal.h"

#include <BAN/Debug.h>
#include <BAN/UTF8.h>

#include <LibClipboard/Clipboard.h>

#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

static constexpr uint32_t s_colors_dark[] {
	0xCC'000000,
	0xCC'FF0000,
	0xCC'00FF00,
	0xCC'FFFF00,
	0xCC'0000FF,
	0xCC'FF00FF,
	0xCC'00FFFF,
	0xCC'BFBFBF,
};

static constexpr uint32_t s_colors_bright[] {
	0xCC'3F3F3F,
	0xCC'FF7F7F,
	0xCC'7FFF7F,
	0xCC'FFFF7F,
	0xCC'7F7FFF,
	0xCC'FF7FFF,
	0xCC'7FFFFF,
	0xCC'FFFFFF,
};

static constexpr auto s_default_bg_color = s_colors_dark[0];
static constexpr auto s_default_fg_color = s_colors_bright[7];

void Terminal::start_shell()
{
	int pts_master = posix_openpt(O_RDWR | O_NOCTTY);
	if (pts_master == -1)
	{
		dwarnln("posix_openpt: {}", strerror(errno));
		exit(1);
	}

	if (grantpt(pts_master) == -1)
	{
		dwarnln("grantpt: {}", strerror(errno));
		exit(1);
	}

	if (unlockpt(pts_master) == -1)
	{
		dwarnln("unlockpt: {}", strerror(errno));
		exit(1);
	}

	pid_t shell_pid = fork();
	if (shell_pid == 0)
	{
		if (setsid() == -1)
		{
			dwarnln("setsid: {}", strerror(errno));
			exit(1);
		}

		char* pts_slave_name = ptsname(pts_master);
		if (pts_slave_name == nullptr)
		{
			dwarnln("ptsname: {}", strerror(errno));
			exit(1);
		}

		int pts_slave = open(pts_slave_name, O_RDWR);
		if (pts_slave == -1)
		{
			dwarnln("open: {}", strerror(errno));
			exit(1);
		}

		termios termios;
		if (tcgetattr(pts_slave, &termios) == -1)
			dwarnln("tcgetattr: {}", strerror(errno));
		else
		{
			termios.c_oflag |= OPOST | ONLCR;
			if (tcsetattr(pts_slave, TCSANOW, &termios) == -1)
				dwarnln("tcsetattr: {}", strerror(errno));
		}

		if (dup2(pts_slave, STDIN_FILENO) == -1 || dup2(pts_slave, STDOUT_FILENO) == -1 || dup2(pts_slave, STDERR_FILENO) == -1)
		{
			dwarnln("dup2: {}", strerror(errno));
			exit(1);
		}

		close(pts_slave);
		close(pts_master);

		setenv("TERM",      "ansi-color", 1);
		setenv("COLORTERM", "truecolor",  1);

		const char* shell = []() -> const char* {
			const auto* pwd = getpwuid(getuid());
			if (pwd && pwd->pw_shell)
				return pwd->pw_shell;
			return "/bin/sh";
		}();

		execl(shell, shell, NULL);
		exit(1);
	}

	if (shell_pid == -1)
	{
		dwarnln("fork: {}", strerror(errno));
		exit(1);
	}

	m_shell_info = {
		.pts_master = pts_master,
		.pid = shell_pid
	};
}

static volatile bool s_shell_exited = false;

static uint64_t get_current_ms()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1'000 + ts.tv_nsec / 1'000'000;
}

void Terminal::run()
{
	signal(SIGCHLD, [](int) { s_shell_exited = true; });
	start_shell();

	m_bg_color = s_default_bg_color;
	m_fg_color = s_default_fg_color;

	auto attributes = LibGUI::Window::default_attributes;
	attributes.shown = false;
	attributes.alpha_channel = true;
	attributes.resizable = true;

	m_window = MUST(LibGUI::Window::create(600, 400, "Terminal"_sv, attributes));
	m_window->texture().fill(s_default_bg_color);
	m_window->texture().set_bg_color(s_default_bg_color);
	m_window->invalidate();

	attributes.shown = true;
	m_window->set_attributes(attributes);

	m_font = MUST(LibFont::Font::load("/usr/share/fonts/lat0-16.psfu"_sv));

	m_window->set_min_size(m_font.width() * 8, m_font.height() * 2);

	MUST(m_cells.resize(rows() * cols(), {
		.codepoint = 0,
		.fg_color = s_default_fg_color,
		.bg_color = s_default_bg_color,
		.bold = false,
	}));
	m_cells_rows = rows();
	m_cells_cols = cols();

	MUST(m_soft_wraps.resize(rows(), false));

	m_scrollback = MUST(BAN::UniqPtr<Scrollback>::create());

	{
		const winsize winsize {
			.ws_row = static_cast<unsigned short>(rows()),
			.ws_col = static_cast<unsigned short>(cols()),
			.ws_xpixel = static_cast<unsigned short>(m_window->width()),
			.ws_ypixel = static_cast<unsigned short>(m_window->height()),
		};
		if (tcsetwinsize(m_shell_info.pts_master, &winsize) == -1)
			perror("tcsetwinsize");
	}

	m_cursor_blink_toggle = true;
	m_cursor_blink_next_ms = get_current_ms() + m_cursor_blink_interval_ms;

	show_cursor();

	Rectangle invalidate;

	m_window->set_resize_window_event_callback([&] {
		const auto is_cell_visible = [](const Cell& cell) {
			return cell.codepoint != 0 || cell.bg_color != s_default_bg_color;
		};

		const auto blank_cell = Cell {
			.codepoint = 0,
			.fg_color = s_default_fg_color,
			.bg_color = s_default_bg_color,
			.bold = false,
		};

		if (cols() != m_cells_cols && !m_scrollback->empty())
		{
			auto new_scrollback_uptr = MUST(BAN::UniqPtr<Scrollback>::create());

			auto& new_scrollback = *new_scrollback_uptr;

			const auto push_empty_line = [&] {
				if (!new_scrollback.full())
					new_scrollback.push({ .cells = { cols(), blank_cell }, .soft_wrap = false });
				else
				{
					auto temp = BAN::move(new_scrollback.front());
					for (auto& cell : temp.cells)
						cell = blank_cell;
					temp.soft_wrap = false;

					new_scrollback.pop();
					new_scrollback.push(BAN::move(temp));
				}
			};

			uint32_t cur_x = 0;
			const auto put_cell = [&](const Cell& cell) {
				if (cur_x >= new_scrollback.back().cells.size())
				{
					new_scrollback.back().soft_wrap = true;
					push_empty_line();
					cur_x = 0;
				}

				new_scrollback.back().cells[cur_x++] = cell;
			};

			push_empty_line();

			const auto& scrollback = *m_scrollback;
			for (size_t y = 0; y < scrollback.size(); y++)
			{
				const auto& line = scrollback[y];

				for (size_t x = 0; x < line.cells.size(); x++)
				{
					if (!is_cell_visible(line.cells[x]))
						continue;

					for (size_t xoff = 1; xoff <= x; xoff++)
					{
						const auto& prev_cell = line.cells[x - xoff];
						if (is_cell_visible(prev_cell))
							break;
						put_cell(prev_cell);
					}

					put_cell(line.cells[x]);
				}

				if (!line.soft_wrap && y + 1 < scrollback.size())
				{
					push_empty_line();
					cur_x = 0;
				}
			}

			m_scrollback = BAN::move(new_scrollback_uptr);
		}

		if (cols() != m_cells_cols || rows() != m_cells_rows)
		{
			BAN::Vector<Cell> new_cells;
			MUST(new_cells.resize(rows() * cols(), blank_cell));

			BAN::Vector<bool> new_soft_wraps;
			MUST(new_soft_wraps.resize(rows(), false));

			auto new_cursor = m_cursor;

			uint32_t cur_x = 0, cur_y = 0;
			const auto put_cell = [&](const Cell& cell) {
				if (cur_x >= cols())
				{
					new_soft_wraps[cur_y] = true;
					cur_x = 0;
					cur_y++;
				}

				if (cur_y >= rows())
				{
					const size_t scroll = cur_y - rows() + 1;

					memmove(&new_cells[0], &new_cells[cols()], (rows() - scroll) * cols() * sizeof(Cell));
					for (size_t i = (rows() - scroll) * cols(); i < rows() * cols(); i++)
						new_cells[i] = blank_cell;

					memmove(&new_soft_wraps[0], &new_soft_wraps[scroll], rows() - scroll);
					memset(&new_soft_wraps[rows() - scroll], 0, scroll);

					new_cursor.y -= scroll;
					cur_y -= scroll;
				}

				new_cells[cur_y * cols() + cur_x++] = cell;
			};

			const size_t visible_height = ({
				size_t height = m_cells_rows;
				for (; height > 0; height--)
				{
					bool is_visible = false;
					for (size_t i = (height - 1) * m_cells_cols; i < m_cells_rows * m_cells_cols && !is_visible; i++)
						is_visible = is_cell_visible(m_cells[i]);
					if (is_visible)
						break;
				}
				height;
			});

			for (size_t y = 0; y < visible_height; y++)
			{
				for (size_t x = 0; x < m_cells_cols; x++)
				{
					if (x == m_cursor.x && y == m_cursor.y)
						new_cursor = { .x = cur_x, .y = cur_y };

					const auto& cell = m_cells[y * m_cells_cols + x];
					if (!is_cell_visible(cell))
						continue;

					for (size_t xoff = 1; xoff <= x; xoff++)
					{
						const auto& prev_cell = m_cells[y * m_cells_cols + x - xoff];
						if (is_cell_visible(prev_cell))
							break;
						put_cell(prev_cell);
					}

					put_cell(cell);
				}

				if (!m_soft_wraps[y])
				{
					cur_x = 0;
					cur_y++;
				}
			}

			m_cells = BAN::move(new_cells);
			m_cells_rows = rows();
			m_cells_cols = cols();
			m_soft_wraps = BAN::move(new_soft_wraps);
			m_cursor = new_cursor;

			m_is_selecting = false;
			m_cur_selection = {};

			m_visual_scroll = BAN::Math::min(m_visual_scroll, m_scrollback->size());
			invalidate = invalidate.get_bounding_box(render_visual_scroll());
		}

		const winsize winsize {
			.ws_row = static_cast<unsigned short>(rows()),
			.ws_col = static_cast<unsigned short>(cols()),
			.ws_xpixel = static_cast<unsigned short>(m_window->width()),
			.ws_ypixel = static_cast<unsigned short>(m_window->height()),
		};
		if (tcsetwinsize(m_shell_info.pts_master, &winsize) == -1)
			perror("tcsetwinsize");
	});

	m_window->set_window_focus_event_callback([&](LibGUI::EventPacket::WindowFocusEvent::event_t event) {
		if (event.focused)
		{
			m_cursor_blink_next_ms = get_current_ms() + m_cursor_blink_interval_ms;
			m_cursor_blink_toggle = true;
		}
		else
		{
			m_cursor_blink_next_ms = UINT64_MAX;
			m_cursor_blink_toggle = false;
		}
	});

	m_window->set_key_event_callback([&](LibGUI::EventPacket::KeyEvent::event_t event) {
		if (event.released())
			return;

		if (event.shift() && event.ctrl())
		{
			switch (event.key)
			{
				case LibInput::Key::C:
				{
					if (!m_cur_selection.has_value())
						break;

					const uint32_t selection_start = m_cur_selection->start.y * cols() + m_cur_selection->start.x;
					const uint32_t selection_end   = m_cur_selection->  end.y * cols() + m_cur_selection->  end.x;

					uint32_t selection_min = BAN::Math::min(selection_start, selection_end);
					for (; selection_min < m_cells.size(); selection_min++)
						if (m_cells[selection_min].codepoint != 0)
							break;

					uint32_t selection_max = BAN::Math::max(selection_start, selection_end);
					for (; selection_max >= selection_min; selection_max--)
						if (m_cells[selection_max].codepoint != 0)
							break;

					BAN::String buffer;
					for (size_t i = selection_min; i <= selection_max; i++)
					{
						if (i != selection_min && i % cols() == 0 && !m_soft_wraps[i / cols() - 1])
							MUST(buffer.push_back('\n'));

						const auto cell = m_cells[i];
						if (cell.codepoint == 0)
							continue;

						char bytes[5];
						BAN::UTF8::from_codepoints(&cell.codepoint, 1, bytes);
						MUST(buffer.append(bytes));
					}

					if (auto ret = LibClipboard::Clipboard::set_clipboard_text(buffer); ret.is_error())
						dwarnln("Failed to set clipboard: {}", ret.error());

					break;
				}
				case LibInput::Key::V:
				{
					auto clipboard_or_error = LibClipboard::Clipboard::get_clipboard_text();
					if (clipboard_or_error.is_error())
					{
						dwarnln("Failed to get clipboard: {}", clipboard_or_error.error());
						break;
					}

					auto clipboard = clipboard_or_error.release_value();
					if (!clipboard.empty())
					{
						if (m_brackted_paste_mode)
							write_shell("\e[200~"_sv);
						write_shell(clipboard);
						if (m_brackted_paste_mode)
							write_shell("\e[201~"_sv);
					}

					break;
				}
				default:
					break;
			}

			return;
		}

		if (const char* text = LibInput::key_to_utf8_ansi(event.key, event.modifier))
		{
			write_shell(text);

			if (m_visual_scroll != 0)
			{
				m_visual_scroll = 0;
				invalidate = invalidate.get_bounding_box(render_visual_scroll());
			}
		}

		m_cursor_blink_next_ms = get_current_ms() + m_cursor_blink_interval_ms;
		m_cursor_blink_toggle = true;
	});

	m_window->set_mouse_move_event_callback([&](LibGUI::EventPacket::MouseMoveEvent::event_t event) {
		if (m_visual_scroll != 0)
			return;

		if (!m_is_selecting)
			return;

		m_cur_selection->end = {
			.x = static_cast<uint32_t>(BAN::Math::clamp<int32_t>(event.x / static_cast<int32_t>(m_font.width()),  0, cols() - 1)),
			.y = static_cast<uint32_t>(BAN::Math::clamp<int32_t>(event.y / static_cast<int32_t>(m_font.height()), 0, rows() - 1)),
		};

		invalidate = invalidate.get_bounding_box(show_selection());
	});

	m_window->set_mouse_button_event_callback([&](LibGUI::EventPacket::MouseButtonEvent::event_t event) {
		if (m_visual_scroll != 0)
			return;

		switch (event.button)
		{
			case LibInput::MouseButton::Right:
				if (!m_is_selecting)
					m_cur_selection = {};
				m_is_selecting = false;
				break;
			case LibInput::MouseButton::Left:
				if (event.pressed)
				{
					m_cur_selection = Selection {};
					m_cur_selection->start = m_cur_selection->end = {
						.x = static_cast<uint32_t>(BAN::Math::clamp<int32_t>(event.x / static_cast<int32_t>(m_font.width()),  0, cols() - 1)),
						.y = static_cast<uint32_t>(BAN::Math::clamp<int32_t>(event.y / static_cast<int32_t>(m_font.height()), 0, rows() - 1)),
					};
				}
				m_is_selecting = event.pressed;
				break;
			default:
				return;
		}

		invalidate = invalidate.get_bounding_box(show_selection());
	});

	m_window->set_mouse_scroll_event_callback([&](LibGUI::EventPacket::MouseScrollEvent::event_t event) {
		m_is_selecting = false;
		m_cur_selection = {};

		m_visual_scroll = BAN::Math::clamp<int32_t>(static_cast<int32_t>(m_visual_scroll) + event.scroll * 5, 0, m_scrollback->size());
		invalidate = invalidate.get_bounding_box(render_visual_scroll());
	});

	const int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1)
	{
		dwarnln("epoll_create1: {}", strerror(errno));
		return;
	}

	{
		const int fds[] { m_shell_info.pts_master, m_window->server_fd() };
		for (const int fd : fds)
		{
			epoll_event event {
				.events = EPOLLIN,
				.data = { .fd = fd },
			};
			if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1)
			{
				dwarnln("epoll_ctl: {}", strerror(errno));
				return;
			}
		}
	}

	while (!s_shell_exited)
	{
		const auto timeout_ms = [&]() -> int {
			const uint64_t current_ms = get_current_ms();
			if (current_ms >= m_cursor_blink_next_ms)
				return 0;
			const uint64_t timeout_ms = m_cursor_blink_next_ms - current_ms;
			if (timeout_ms > INT_MAX)
				return -1;
			return timeout_ms;
		}();

		epoll_event events[2];
		const int nevent = epoll_wait(epoll_fd, events, sizeof(events) / sizeof(*events), timeout_ms);
		if (nevent == -1)
		{
			if (errno == EINTR)
				continue;
			dwarnln("epoll_wait: {}", strerror(errno));
			break;
		}

		if (const auto current_ms = get_current_ms(); current_ms >= m_cursor_blink_next_ms)
		{
			const auto toggles = (current_ms - m_cursor_blink_next_ms) / m_cursor_blink_interval_ms + 1;
			m_cursor_blink_next_ms += toggles * m_cursor_blink_interval_ms;
			if (toggles % 2)
				m_cursor_blink_toggle = !m_cursor_blink_toggle;
		}

		invalidate = {};

		invalidate = invalidate.get_bounding_box(hide_cursor());

		for (int i = 0; i < nevent; i++)
		{
			if (!(events[i].events & EPOLLIN))
			{
				s_shell_exited = true;
				break;
			}

			if (events[i].data.fd == m_shell_info.pts_master)
				invalidate = invalidate.get_bounding_box(read_shell());

			if (events[i].data.fd == m_window->server_fd())
				m_window->poll_events();
		}

		invalidate = invalidate.get_bounding_box(show_cursor());

		if (invalidate.width && invalidate.height)
			m_window->invalidate(invalidate.x, invalidate.y, invalidate.width, invalidate.height);
	}
}

Rectangle Terminal::show_selection()
{
	if (m_old_selection.has_value())
	{
		auto& selection = m_old_selection.value();
		selection.start.x = BAN::Math::min(selection.start.x, cols() - 1);
		selection.start.y = BAN::Math::min(selection.start.y, rows() - 1);
		selection.  end.x = BAN::Math::min(selection.  end.x, cols() - 1);
		selection.  end.y = BAN::Math::min(selection.  end.y, rows() - 1);
	}

	Rectangle invalidate;

	const auto redraw_character = [this, &invalidate](size_t index, bool selected) {
		const uint32_t col = index % cols();
		const uint32_t row = index / cols();
		const uint32_t x = col * m_font.width();
		const uint32_t y = row * m_font.height();
		const auto& cell = m_cells[row * cols() + col];
		if (selected && cell.codepoint == 0)
			return;
		m_window->texture().fill_rect(x, y, m_font.width(), m_font.height(), selected ? s_default_fg_color : cell.bg_color);
		m_window->texture().draw_character(cell.codepoint, m_font, x, y, selected ? s_default_bg_color : cell.fg_color);
		invalidate = invalidate.get_bounding_box({ x, y, m_font.width(), m_font.height() });
	};

	if (m_old_selection.has_value())
	{
		const uint32_t old_selection_start = m_old_selection->start.y * cols() + m_old_selection->start.x;
		const uint32_t old_selection_end   = m_old_selection->  end.y * cols() + m_old_selection->  end.x;
		const auto old_selection_min = BAN::Math::min(old_selection_start, old_selection_end);
		const auto old_selection_max = BAN::Math::max(old_selection_start, old_selection_end);
		for (size_t i = old_selection_min; i <= old_selection_max; i++)
			redraw_character(i, false);
	}

	if (m_cur_selection.has_value())
	{
		const uint32_t cur_selection_start = m_cur_selection->start.y * cols() + m_cur_selection->start.x;
		const uint32_t cur_selection_end   = m_cur_selection->  end.y * cols() + m_cur_selection->  end.x;
		const auto cur_selection_min = BAN::Math::min(cur_selection_start, cur_selection_end);
		const auto cur_selection_max = BAN::Math::max(cur_selection_start, cur_selection_end);
		for (size_t i = cur_selection_min; i <= cur_selection_max; i++)
			redraw_character(i, true);
	}

	m_old_selection = m_cur_selection;

	return invalidate;
}

Rectangle Terminal::hide_cursor()
{
	if (m_cursor.x >= cols())
		return {};
	if (m_cursor.y + m_visual_scroll >= rows())
		return {};

	return render_cell(
		m_cursor.x,
		m_cursor.y + m_visual_scroll,
		m_cells[m_cursor.y * cols() + m_cursor.x]
	);
}

Rectangle Terminal::show_cursor()
{
	if (!m_cursor_shown || !m_cursor_blink_toggle)
		return {};

	if (m_cursor.x >= cols())
		return {};
	if (m_cursor.y + m_visual_scroll >= rows())
		return {};

	const uint32_t y_offset = m_font.height() * 13 / 16;

	Rectangle rect;
	rect.x      =  m_cursor.x                    * m_font.width();
	rect.y      = (m_cursor.y + m_visual_scroll) * m_font.height() + y_offset;
	rect.width  = m_font.width();
	rect.height = m_font.height() - y_offset - 1;

	m_window->texture().fill_rect(rect.x, rect.y, rect.width, rect.height, 0xFFFFFFFF);

	return rect;
}

void Terminal::write_shell(BAN::StringView data)
{
	size_t total_written = 0;
	while (total_written < data.size())
	{
		const ssize_t nwrite = write(m_shell_info.pts_master, data.data() + total_written, data.size() - total_written);
		if (nwrite < 0)
			dwarnln("write: {}", strerror(errno));
		if (nwrite <= 0)
		{
			s_shell_exited = true;
			return;
		}
		total_written += nwrite;
	}
}

Rectangle Terminal::read_shell()
{
	char buffer[512];
	const ssize_t nread = read(m_shell_info.pts_master, buffer, sizeof(buffer));
	if (nread < 0)
		dwarnln("read: {}", strerror(errno));
	if (nread <= 0)
	{
		s_shell_exited = true;
		return {};
	}

	Rectangle invalidate;
	for (ssize_t i = 0; i < nread; i++)
		invalidate = invalidate.get_bounding_box(putchar(buffer[i]));
	return invalidate;
}

void Terminal::handle_sgr(int32_t value)
{
	switch (value)
	{
		case -1: case 0:
			m_bg_color = s_default_bg_color;
			m_fg_color = s_default_fg_color;
			m_colors_inverted = false;
			m_is_bold = false;
			break;
		case 1:
			m_is_bold = true;
			break;
		case 7:
			m_colors_inverted = true;
			break;
		case 10:
			// default font
			break;
		case 22:
			m_is_bold = false;
			break;
		case 27:
			m_colors_inverted = false;
			break;
		case 30 ... 37:
			m_fg_color = s_colors_dark[value - 30];
			break;
		case 39:
			m_fg_color = s_default_fg_color;
			break;
		case 40 ... 47:
			m_bg_color = s_colors_dark[value - 40];
			break;
		case 49:
			m_bg_color = s_default_bg_color;
			break;
		case 90 ... 97:
			m_fg_color = s_colors_bright[value - 90];
			break;
		case 100 ... 107:
			m_bg_color = s_colors_bright[value - 100];
			break;
		default:
			dprintln("TODO: SGR {}", value);
			break;
	}
}

BAN::Optional<uint32_t> Terminal::get_8bit_color()
{
	ASSERT(m_csi_info.fields[1] == 5);
	if (m_csi_info.fields[2] < 0)
		return {};

	const uint8_t code = BAN::Math::min(m_csi_info.fields[2], 255);
	if (code < 8)
		return s_colors_dark[code];
	if (code < 16)
		return s_colors_bright[code - 8];

	if (code < 232)
	{
		const uint8_t r = (code - 16) / 36 % 6 * 40 + 55;
		const uint8_t g = (code - 16) /  6 % 6 * 40 + 55;
		const uint8_t b = (code - 16) /  1 % 6 * 40 + 55;
		return b | (g << 8) | (r << 16) | (0xCC << 24);
	}

	const uint8_t gray = (code - 232) * 10 + 8;
	return gray | (gray << 8) | (gray << 16) | (0xCC << 24);
}

BAN::Optional<uint32_t> Terminal::get_24bit_color()
{
	ASSERT(m_csi_info.fields[1] == 2);
	if (m_csi_info.fields[2] < 0) return {};
	if (m_csi_info.fields[3] < 0) return {};
	if (m_csi_info.fields[4] < 0) return {};
	const uint8_t r = BAN::Math::min(m_csi_info.fields[2], 255);
	const uint8_t g = BAN::Math::min(m_csi_info.fields[3], 255);
	const uint8_t b = BAN::Math::min(m_csi_info.fields[4], 255);
	return b | (g << 8) | (r << 16) | (0xCC << 24);
}

Rectangle Terminal::handle_csi(char ch)
{
	if (ch == ';')
	{
		m_csi_info.index++;
		return {};
	}

	if (ch == '?' || ch == '>' || ch == '=' || ch == '%')
	{
		m_csi_info.param = ch;
		return {};
	}

	if (isdigit(ch))
	{
		if (m_csi_info.index < m_csi_info.max_fields)
		{
			auto& field = m_csi_info.fields[m_csi_info.index];
			field = (BAN::Math::max(field, 0) * 10) + (ch - '0');
		}
		return {};
	}

	Rectangle invalidate;

	auto& texture = m_window->texture();

	const auto fg_color = m_colors_inverted ? m_bg_color : m_fg_color;
	const auto bg_color = m_colors_inverted ? m_fg_color : m_bg_color;

	switch (m_csi_info.param)
	{
	case 0:
		switch (ch)
		{
		case 'A':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.y = BAN::Math::max<int32_t>(m_cursor.y - m_csi_info.fields[0], 0);
			break;
		case 'B':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.y = BAN::Math::min<int32_t>(m_cursor.y + m_csi_info.fields[0], rows() - 1);
			break;
		case 'C':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.x = BAN::Math::min<int32_t>(m_cursor.x + m_csi_info.fields[0], cols() - 1);
			break;
		case 'D':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.x = BAN::Math::max<int32_t>(m_cursor.x - m_csi_info.fields[0], 0);
			break;
		case 'E':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.y = BAN::Math::min<int32_t>(m_cursor.y + m_csi_info.fields[0], rows() - 1);
			m_cursor.x = 0;
			break;
		case 'F':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			m_cursor.y = BAN::Math::max<int32_t>(m_cursor.y - m_csi_info.fields[0], 0);
			m_cursor.x = 0;
			break;
		case 'G':
			m_cursor.x = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, cols()) - 1;
			break;
		case 'H':
			m_cursor.x = BAN::Math::clamp<int32_t>(m_csi_info.fields[1], 1, cols()) - 1;
			m_cursor.y = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, rows()) - 1;
			break;
		case 'J':
		{
			m_csi_info.fields[0] = BAN::Math::max(m_csi_info.fields[0], 0);

			const uint32_t s_idx = (m_csi_info.fields[0] == 0) ? m_cursor.y * cols() + m_cursor.x : 0;
			const uint32_t e_idx = (m_csi_info.fields[0] == 1) ? m_cursor.y * cols() + m_cursor.x : cols() * rows() - 1;
			for (size_t i = s_idx; i <= e_idx; i++)
				m_cells[i] = {
					.codepoint = 0,
					.fg_color = fg_color,
					.bg_color = bg_color,
					.bold = false,
				};

			if (m_csi_info.fields[0] == 3)
			{
				while (!m_scrollback->empty())
					m_scrollback->pop();
				m_visual_scroll = 0;

				texture.fill(bg_color);
				invalidate = { 0, 0, m_window->width(), m_window->height() };

				break;
			}

			if (m_visual_scroll != 0)
			{
				invalidate = render_visual_scroll(true);
				break;
			}

			Rectangle rects[2];
			size_t rect_count = 0;

			if (m_csi_info.fields[0] == 0)
			{
				rects[0].x      = m_cursor.x * m_font.width();
				rects[0].y      = m_cursor.y * m_font.height();
				rects[0].width  = m_window->width() - rects[0].x;
				rects[0].height = m_font.height();

				rects[1].x      = 0;
				rects[1].y      = (m_cursor.y + 1) * m_font.height();
				rects[1].width  = m_window->width();
				rects[1].height = m_window->height() - rects[1].y;

				rect_count = 2;
			}
			else if (m_csi_info.fields[0] == 1)
			{
				rects[0].x      = 0;
				rects[0].y      = m_cursor.y * m_font.height();
				rects[0].width  = m_cursor.x * m_font.width();
				rects[0].height = m_font.height();

				rects[1].x      = 0;
				rects[1].y      = 0;
				rects[1].width  = m_window->width();
				rects[1].height = m_cursor.y * m_font.height();

				rect_count = 2;
			}
			else
			{
				rects[0].x      = 0;
				rects[0].y      = 0;
				rects[0].width  = m_window->width();
				rects[0].height = m_window->height();

				rect_count = 1;
			}

			for (size_t i = 0; i < rect_count; i++)
			{
				texture.fill_rect(rects[i].x, rects[i].y, rects[i].width, rects[i].height, bg_color);
				invalidate = invalidate.get_bounding_box(rects[i]);
			}

			break;
		}
		case 'K':
		{
			m_csi_info.fields[0] = BAN::Math::max(m_csi_info.fields[0], 0);

			const uint32_t s_col = (m_csi_info.fields[0] == 0) ? m_cursor.x : 0;
			const uint32_t e_col = (m_csi_info.fields[0] == 1) ? m_cursor.x : cols() - 1;
			for (size_t i = s_col; i <= e_col; i++)
				m_cells[m_cursor.y * cols() + i] = {
					.codepoint = 0,
					.fg_color = fg_color,
					.bg_color = bg_color,
					.bold = false,
				};

			if (m_visual_scroll != 0)
			{
				invalidate = render_visual_scroll(true);
				break;
			}

			Rectangle rect;
			rect.x      = (m_csi_info.fields[0] == 0) ? m_cursor.x * m_font.width() : 0;
			rect.y      = m_cursor.y * m_font.height();
			rect.width  = (m_csi_info.fields[0] == 1) ? m_cursor.x * m_font.width() : m_window->width() - rect.x;
			rect.height = m_font.height();

			texture.fill_rect(rect.x, rect.y, rect.width, rect.height, bg_color);
			invalidate = rect;

			break;
		}
		case 'L':
		{
			const uint32_t count = (m_csi_info.fields[0] == -1) ? 1 : m_csi_info.fields[0];
			const uint32_t src_y = m_cursor.y * m_font.height();
			const uint32_t dst_y = src_y + count * m_font.height();

			for (size_t i = m_cursor.y; i + count < rows(); i++)
				for (size_t col = 0; col < cols(); col++)
					m_cells[(i + count) * cols() + col] = m_cells[i * cols() + col];
			for (size_t row = m_cursor.y; row < m_cursor.y + count; row++)
				for (size_t col = 0; col < cols(); col++)
					m_cells[row * cols() + col] = {
						.codepoint = 0,
						.fg_color = fg_color,
						.bg_color = bg_color,
						.bold = false,
					};

			if (m_visual_scroll != 0)
			{
				invalidate = render_visual_scroll(true);
				break;
			}

			texture.copy_horizontal_slice(dst_y, src_y, m_window->height() - dst_y);
			texture.fill_rect(0, src_y, m_window->width(), count * m_font.height(), bg_color);
			invalidate = {
				0,
				src_y,
				m_window->width(),
				m_window->height() - src_y
			};

			break;
		}
		case 'M':
		{
			const uint32_t count = (m_csi_info.fields[0] == -1) ? 1 : m_csi_info.fields[0];
			const uint32_t dst_y = m_cursor.y * m_font.height();
			const uint32_t src_y = dst_y + count * m_font.height();

			for (size_t i = m_cursor.y; i + count < rows(); i++)
				for (size_t col = 0; col < cols(); col++)
					m_cells[i * cols() + col] = m_cells[(i + count) * cols() + col];
			for (size_t row = m_cursor.y + count; row < rows(); row++)
				for (size_t col = 0; col < cols(); col++)
					m_cells[row * cols() + col] = {
						.codepoint = 0,
						.fg_color = fg_color,
						.bg_color = bg_color,
						.bold = false,
					};

			if (m_visual_scroll != 0)
			{
				invalidate = render_visual_scroll(true);
				break;
			}

			texture.copy_horizontal_slice(dst_y, src_y, m_window->height() - dst_y);
			texture.fill_rect(0, m_window->height() - count * m_font.height(), m_window->width(), count * m_font.height(), bg_color);
			invalidate = {
				0,
				src_y,
				m_window->width(),
				m_window->height() - src_y
			};

			break;
		}
		case 'P':
		{
			const uint32_t count = (m_csi_info.fields[0] == -1) ? 1 : m_csi_info.fields[0];
			const uint32_t dst_x = m_cursor.x * m_font.width();
			const uint32_t src_x = (m_cursor.x + count) * m_font.width();
			const uint32_t y = m_cursor.y * m_font.height();

			for (size_t i = m_cursor.x; i + count < cols(); i++)
				m_cells[m_cursor.y * cols() + i] = m_cells[m_cursor.y * cols() + i + count];
			for (size_t i = m_cursor.x + count; i < cols(); i++)
				m_cells[m_cursor.y * cols() + i] = {
					.codepoint = 0,
					.fg_color = fg_color,
					.bg_color = bg_color,
					.bold = false,
				};

			if (m_visual_scroll != 0)
			{
				invalidate = render_visual_scroll(true);
				break;
			}

			texture.copy_rect(dst_x, y, src_x, y, m_window->width() - src_x, m_font.height());
			texture.fill_rect(m_window->width() - count * m_font.width(), y, count * m_font.width(), m_font.height(), bg_color);
			invalidate = {
				dst_x,
				y,
				m_window->width() - dst_x,
				m_font.height()
			};

			break;
		}
		case '@':
		{
			const uint32_t count = (m_csi_info.fields[0] == -1) ? 1 : m_csi_info.fields[0];
			const uint32_t dst_x = (m_cursor.x + count) * m_font.width();
			const uint32_t src_x = m_cursor.x * m_font.width();
			const uint32_t y = m_cursor.y * m_font.height();

			for (size_t i = m_cursor.x; i + count < cols(); i++)
				m_cells[m_cursor.y * cols() + i + count] = m_cells[m_cursor.y * cols() + i];
			for (size_t i = m_cursor.x; i < m_cursor.x + count; i++)
				m_cells[m_cursor.y * cols() + i] = {
					.codepoint = 0,
					.fg_color = fg_color,
					.bg_color = bg_color,
					.bold = false,
				};

			if (m_visual_scroll != 0)
			{
				invalidate = render_visual_scroll(true);
				break;
			}

			texture.copy_rect(dst_x, y, src_x, y, m_window->width() - dst_x, m_font.height());
			texture.fill_rect(src_x, y, count * m_font.width(), m_font.height(), bg_color);
			invalidate = {
				src_x,
				y,
				m_window->width() - src_x,
				m_font.height()
			};

			break;
		}
		case 'b':
			if (m_csi_info.fields[0] == -1)
				m_csi_info.fields[0] = 1;
			if (m_last_graphic_char)
				for (int32_t i = 0; i < m_csi_info.fields[0]; i++)
					invalidate = invalidate.get_bounding_box(putcodepoint(m_last_graphic_char));
			break;
		case 'c':
			write_shell("\e[?1;0c"_sv);
			break;
		case 'd':
			m_cursor.y = BAN::Math::clamp<int32_t>(m_csi_info.fields[0], 1, rows()) - 1;
			break;
		case 'm':
			if (m_csi_info.fields[0] == 38 || m_csi_info.fields[0] == 48)
			{
				if (m_csi_info.fields[1] != 5 && m_csi_info.fields[1] != 2)
				{
					dprintln("TODO: SGR {}", m_csi_info.fields[1]);
					break;
				}
				const auto color = (m_csi_info.fields[1] == 5)
					? get_8bit_color()
					: get_24bit_color();
				if (color.has_value())
					(m_csi_info.fields[0] == 38 ? m_fg_color : m_bg_color) = *color;
				break;
			}
			for (size_t i = 0; i <= m_csi_info.index && i < m_csi_info.max_fields; i++)
				handle_sgr(m_csi_info.fields[i]);
			break;
		case 's':
			m_saved_cursor = m_cursor;
			break;
		case 'u':
			m_cursor = m_saved_cursor;
			break;
		case 'n':
			if (m_csi_info.fields[0] != 6)
			{
				dprintln("TODO: CSI {} n", m_csi_info.fields[0]);
				break;
			}
			char buffer[2 + 10 + 1 + 10 + 2];
			sprintf(buffer, "\e[%u;%uR", m_cursor.y + 1, m_cursor.x + 1);
			write_shell(buffer);
			break;
		default:
			dprintln("TODO: CSI {}", ch);
			break;
		}
		break;
	case '?':
		switch (ch)
		{
		case 'h':
		case 'l':
			switch (m_csi_info.fields[0])
			{
			case 25:
				m_cursor_shown = (ch == 'h');
				break;
			case 2004:
				m_brackted_paste_mode = (ch == 'h');
				break;
			default:
				dprintln("TODO: CSI ? {} {}", m_csi_info.fields[0], ch);
				break;
			}
			break;
		default:
			dprintln("TODO: CSI ? {}", ch);
			break;
		}
		break;
	case '>':
		switch (ch)
		{
		case 'c':
			write_shell("\e[>0;10;1c"_sv);
			break;
		default:
			dprintln("TODO: CSI > {}", ch);
			break;
		}
		break;
	case '=':
		switch (ch)
		{
		case 'c':
			write_shell("\eP!|00000000\e\\"_sv);
			break;
		default:
			dprintln("TODO: CSI = {}", ch);
			break;
		}
		break;
	case '%':
		switch (ch)
		{
		default:
			dprintln("TODO: CSI % {}", ch);
			break;
		}
		break;
	}

	m_state = State::Normal;
	return invalidate;
}

Rectangle Terminal::render_cell(uint32_t x, uint32_t y, const Cell& cell)
{
	auto& texture = m_window->texture();

	const auto tex_x = x * m_font.width();
	const auto tex_y = y * m_font.height();

	const auto w = (x < cols() - 1) ? m_font.width()  : texture.width()  - tex_x;
	const auto h = (y < rows() - 1) ? m_font.height() : texture.height() - tex_y;

	texture.fill_rect(tex_x, tex_y, w, h, cell.bg_color);

	if (cell.codepoint != 0 && cell.codepoint != ' ' && cell.fg_color != cell.bg_color)
	{
		texture.draw_character(cell.codepoint, m_font, tex_x, tex_y, cell.fg_color);
		if (cell.bold)
			texture.draw_character(cell.codepoint, m_font, tex_x + 1, tex_y, cell.fg_color);
	}

	return { tex_x, tex_y, w, h };
}

Rectangle Terminal::render_visual_scroll(bool skip_scrollback)
{
	Rectangle rect1, rect2;

	if (!skip_scrollback)
	{
		for (size_t y = 0; y < BAN::Math::min<size_t>(m_visual_scroll, rows()); y++)
		{
			const auto& line = (*m_scrollback)[m_scrollback->size() - (m_visual_scroll - y)];
			for (size_t x = 0; x < cols(); x++)
				render_cell(x, y, line.cells[x]);
		}

		rect1.x      = 0;
		rect1.y      = 0;
		rect1.width  = m_window->width();
		rect1.height = m_visual_scroll >= rows() ? m_window->height() : m_visual_scroll * m_font.height();
	}

	if (m_visual_scroll < rows())
	{
		for (size_t y = 0; y < rows() - m_visual_scroll; y++)
			for (size_t x = 0; x < cols(); x++)
				render_cell(x, m_visual_scroll + y, m_cells[y * cols() + x]);

		rect2.x      = 0;
		rect2.y      = (rows() - m_visual_scroll) * m_font.height();
		rect2.width  = m_window->width();
		rect2.height = m_window->height() - rect2.y;
	}

	return rect1.get_bounding_box(rect2);
}

Rectangle Terminal::handle_scroll()
{
	ASSERT(m_cursor.y == rows());

	auto& scrollback = *m_scrollback;
	if (!scrollback.full())
		scrollback.push({ .cells = { cols() }, .soft_wrap = false });
	else
	{
		auto temp = BAN::move(scrollback.front());
		scrollback.pop();
		scrollback.push(BAN::move(temp));
	}
	memcpy(scrollback.back().cells.data(), m_cells.data(), cols() * sizeof(Cell));
	scrollback.back().soft_wrap = m_soft_wraps[0];

	memmove(&m_cells[0], &m_cells[cols()], (rows() - 1) * cols() * sizeof(Cell));
	for (size_t i = (rows() - 1) * cols(); i < rows() * cols(); i++)
		m_cells[i] = {
			.codepoint = 0,
			.fg_color = m_fg_color,
			.bg_color = m_bg_color,
			.bold = false,
		};
	memmove(m_soft_wraps.data(), m_soft_wraps.data() + 1, rows() - 1);
	m_soft_wraps[rows() - 1] = false;

	m_cursor.y--;

	if (m_visual_scroll != 0)
	{
		if (m_visual_scroll >= scrollback.size())
			return render_visual_scroll();

		m_visual_scroll++;
		if (m_visual_scroll >= rows())
			return {};
		return render_visual_scroll(false);
	}

	auto& texture = m_window->texture();

	texture.shift_vertical(-static_cast<int32_t>(m_font.height()));

	const size_t y_off = (rows() - 1) * m_font.height();
	texture.fill_rect(0, y_off, m_window->width(), m_window->height() - y_off, m_bg_color);

	if (m_cur_selection.has_value())
	{
		m_old_selection = {};

		if (m_cur_selection->start.y == 0 && m_cur_selection->start.y == 0)
			m_cur_selection = {};
		else
		{
			if (m_cur_selection->start.y != 0)
				m_cur_selection->start.y--;
			else
			{
				m_cur_selection->start.y = 0;
				m_cur_selection->start.x = 0;
			}

			if (m_cur_selection->end.y != 0)
				m_cur_selection->end.y--;
			else
			{
				m_cur_selection->end.y = 0;
				m_cur_selection->end.x = 0;
			}
		}

		show_selection();
	}

	return { 0, 0, m_window->width(), m_window->height() };
}

Rectangle Terminal::putcodepoint(uint32_t codepoint)
{
	Rectangle invalidate;

	switch (codepoint)
	{
		case 0x00: // null
		case 0x01: // start of heading
		case 0x02: // start of text
		case 0x1C: // file separator
		case 0x1D: // group separator
		case 0x1E: // record separator
		case 0x1F: // unit separator
			break;
		case '\a':
			// TODO: bell
			break;
		case '\e':
			m_state = State::ESC;
			break;
		case '\n':
			m_cursor.y++;
			if (m_cursor.y >= rows())
				invalidate = handle_scroll();
			break;
		case '\r':
			m_cursor.x = 0;
			break;
		case '\t':
			m_cursor.x++;
			while (m_cursor.x % 8)
				m_cursor.x++;
			break;
		case '\b':
			if (m_cursor.x > 0)
				m_cursor.x--;
			break;
		default:
			if (iscntrl(codepoint))
			{
				dwarnln("unhandled control character 0x{2H}", codepoint);
				break;
			}

			if (m_cursor.x >= cols())
			{
				m_soft_wraps[m_cursor.y] = true;
				m_cursor.x = 0;
				m_cursor.y++;
			}

			if (m_cursor.y >= rows())
				invalidate = handle_scroll();

			const auto cell = Cell {
				.codepoint = codepoint,
				.fg_color = m_colors_inverted ? m_bg_color : m_fg_color,
				.bg_color = m_colors_inverted ? m_fg_color : m_bg_color,
				.bold = m_is_bold,
			};
			m_cells[m_cursor.y * cols() + m_cursor.x] = cell;

			if (m_visual_scroll != 0)
				invalidate = invalidate.get_bounding_box(render_visual_scroll(true));
			else
			{
				const auto cell_area = render_cell(m_cursor.x, m_cursor.y, cell);
				invalidate = invalidate.get_bounding_box(cell_area);
			}

			m_last_graphic_char = codepoint;
			m_cursor.x++;

			break;
	}

	return invalidate;
}

Rectangle Terminal::putchar(uint8_t ch)
{
	if (m_state == State::ESC)
	{
		if (ch != '[')
		{
			dprintln("unknown escape character 0x{2H}", ch);
			m_state = State::Normal;
			return {};
		}
		m_state = State::CSI;
		m_csi_info = {
			.fields = { -1, -1, -1, -1, -1 },
			.index = 0,
			.param = '\0',
		};
		return {};
	}

	if (m_state == State::CSI)
	{
		if (ch < 0x20 || ch > 0xFE)
		{
			dprintln("invalid CSI 0x{2H}", ch);
			m_state = State::Normal;
			return {};
		}
		return handle_csi(ch);
	}

	m_utf8_bytes[m_utf8_index++] = ch;

	const size_t utf8_len = BAN::UTF8::byte_length(m_utf8_bytes[0]);
	if (utf8_len == BAN::UTF8::invalid)
	{
		dwarnln("invalid utf8 leading byte 0x{2H}", ch);
		m_utf8_index = 0;
		return {};
	}
	if (m_utf8_index < utf8_len)
		return {};

	const uint32_t codepoint = BAN::UTF8::to_codepoint(m_utf8_bytes);
	m_utf8_index = 0;

	if (codepoint == BAN::UTF8::invalid)
	{
		char utf8_hex[20];
		char* ptr = utf8_hex;
		for (uint8_t i = 0; i < utf8_len; i++)
		{
			*ptr++ = '0';
			*ptr++ = 'x';
			*ptr++ = (m_utf8_bytes[i] >>  4) < 10 ? (m_utf8_bytes[i] >>  4) + '0' : (m_utf8_bytes[i] >>  4) - 10 + 'A';
			*ptr++ = (m_utf8_bytes[i] & 0xF) < 10 ? (m_utf8_bytes[i] & 0xF) + '0' : (m_utf8_bytes[i] & 0xF) - 10 + 'A';
			*ptr++ = ' ';
		}
		*--ptr = '\0';

		dwarnln("invalid utf8 {}", utf8_hex);
		return {};
	}

	return putcodepoint(codepoint);
}
