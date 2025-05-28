#include <BAN/Vector.h>
#include <BAN/String.h>

#include <ctype.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

struct config_t
{
	bool raw { false };
	bool quit_if_one_screen { false };
	bool no_init { false };
};
static config_t config;

static void usage(const char* argv0, int ret)
{
	FILE* fout = ret ? stderr : stdout;
	fprintf(fout, "usage: %s [OPTIONS]... [--] [FILE]...\n", argv0);
	fprintf(fout, "  -r, -R, --raw            print ANSI control sequences\n");
	fprintf(fout, "  -F, -quit-if-one-screen  exit immediately if output fits in one screen\n");
	fprintf(fout, "  -X, -no-init             don't clear screen at the startup\n");
	fprintf(fout, "  -h, --help               show this message and exit\n");
	exit(ret);
}

static void handle_option_short(const char* argv0, char opt, bool exit_on_error)
{
	switch (opt)
	{
		case 'r':
		case 'R':
			config.raw = true;
			break;
		case 'F':
			config.quit_if_one_screen = true;
			break;
		case 'X':
			config.no_init = true;
			break;
		case 'h':
			usage(argv0, 0);
			break;
		default:
			fprintf(stderr, "unknown option: %c\n", opt);
			fprintf(stderr, "see --help for usage\n");
			if (exit_on_error)
				exit(1);
	}
}

static void handle_option_long(const char* argv0, const char* opt, bool exit_on_error)
{
	if (!strcmp(opt, "--raw"))
		config.raw = true;
	else if (!strcmp(opt, "--quit-if-one-screen"))
		config.quit_if_one_screen = true;
	else if (!strcmp(opt, "--no-init"))
		config.no_init = true;
	else if (!strcmp(opt, "--help"))
		usage(argv0, 0);
	else
	{
		fprintf(stderr, "unknown option: %s\n", opt);
		fprintf(stderr, "see --help for usage\n");
		if (exit_on_error)
			exit(1);
	}
}

static int parse_config_or_exit(int argc, char** argv)
{
	// FIXME: long environment options
	if (const char* env = getenv("LESS"))
		for (size_t i = 0; env[i]; i++)
			(void)handle_option_short(argv[0], env[i], false);

	int i = 1;
	for (; i < argc; i++)
	{
		if (argv[i][0] != '-' || !strcmp(argv[i], "--"))
			break;
		if (argv[i][1] == '-')
			handle_option_long(argv[0], argv[i], true);
		else for (size_t j = 1; argv[i][j]; j++)
			handle_option_short(argv[0], argv[i][j], true);
	}
	return i;
}

static int get_keyboard_fd()
{
	// if stdin is a terminal, use it
	if (isatty(STDIN_FILENO))
		return STDIN_FILENO;

	// otherwise try to open our controlling terminal
	int fd = open("/dev/tty", O_RDONLY);
	if (fd != -1)
		return fd;

	// if that fails, try stderr as the last fallback
	if (isatty(STDERR_FILENO))
		return STDERR_FILENO;

	return -1;
}

static int output_to_non_terminal(int fd)
{
	for (;;)
	{
		char buffer[128];
		const ssize_t nread = read(fd, buffer, sizeof(buffer));
		if (nread == -1)
			perror("read");
		if (nread <= 0)
			break;

		ssize_t total = 0;
		while (total < nread)
		{
			const ssize_t nwrite = write(STDOUT_FILENO, buffer + total, nread - total);
			if (nwrite < 0)
				perror("write");
			if (nwrite <= 0)
				break;
			total += nwrite;
		}
	}

	return 0;
}

static bool read_lines(int fd, winsize ws, BAN::Vector<BAN::String>& out)
{
	char buffer[128];
	const ssize_t nread = read(fd, buffer, sizeof(buffer));
	if (nread < 0)
		perror("read");
	if (nread <= 0)
		return false;

	if (out.empty())
		MUST(out.emplace_back());

	bool in_ansi = false;

	size_t col = 0;
	for (size_t i = 0; i < out.back().size(); i++)
	{
		if (in_ansi)
		{
			if (isalpha(out.back()[i]))
				in_ansi = false;
		}
		else if (out.back()[i] == '\e')
			in_ansi = true;
		else
			col++;
	}

	for (ssize_t i = 0; i < nread; i++)
	{
		if (in_ansi)
		{
			if (config.raw)
				MUST(out.back().push_back(buffer[i]));
			if (isalpha(buffer[i]))
				in_ansi = false;
			continue;
		}

		const auto append_char =
			[&col, &out, ws](char ch)
			{
				if (col >= ws.ws_col)
				{
					MUST(out.emplace_back());
					col = 0;
				}
				MUST(out.back().push_back(ch));
				col++;
			};

		switch (buffer[i])
		{
			case '\e':
				if (config.raw)
					MUST(out.back().push_back(buffer[i]));
				in_ansi = true;
				break;
			case '\n':
				MUST(out.emplace_back());
				col = 0;
				break;
			case '\t':
				append_char(' ');
				while (col % 8)
					append_char(' ');
				break;
			default:
				append_char(buffer[i]);
				break;
		}
	}

	return true;
}

static bool less_file(int fd, int kb_fd, winsize ws)
{
	if (!isatty(STDOUT_FILENO) || kb_fd == -1 || ws.ws_col == 0 || ws.ws_row == 0)
		return output_to_non_terminal(fd);

	BAN::Vector<BAN::String> lines;
	size_t lines_size = 0;
	const auto update_lines_size =
		[&lines, &lines_size]
		{
			lines_size = lines.size();
			if (!lines.empty() && lines.back().empty())
				lines_size--;
		};

	while (lines_size < ws.ws_row)
	{
		if (!read_lines(fd, ws, lines))
			break;
		update_lines_size();
	}

	if (!config.no_init)
	{
		int y = 1;
		if (lines_size < ws.ws_row)
			y = ws.ws_row - lines_size;
		printf("\e[2J\e[%dH", y);
	}

	for (size_t i = 0; i < lines_size && i < static_cast<size_t>(ws.ws_row - 1); i++)
		printf("%s\n", lines[i].data());
	fflush(stdout);

	if (lines_size < ws.ws_row && config.quit_if_one_screen)
		return true;

	printf(":");
	fflush(stdout);

	size_t line = 0;
	for (;;)
	{
		char ch;
		if (read(kb_fd, &ch, 1) != 1)
			break;
		switch (ch)
		{
			case 'q':
				printf("\e[G\e[K");
				fflush(stdout);
				return true;
			case '\e':
			{
				if (read(kb_fd, &ch, 1) != 1 || ch != '[')
					break;
				if (read(kb_fd, &ch, 1) != 1)
					break;
				switch (ch)
				{
					case 'A':
						if (line == 0)
							break;
						line--;

						printf("\e[H");
						for (int i = 0; i < ws.ws_row - 1; i++)
							printf("%s\e[K\n", lines[line + i].data());
						printf(":\e[K");
						fflush(stdout);

						break;
					case 'B':
						while (lines_size - (line + 1) < ws.ws_row)
						{
							if (!read_lines(fd, ws, lines))
								break;
							update_lines_size();
						}
						if (lines_size - (line + 1) < static_cast<size_t>(ws.ws_row - 1))
							break;
						line++;

						printf("\e[H");
						for (int i = 0; i < ws.ws_row - 1; i++)
							printf("%s\e[K\n", lines[line + i].data());
						printf(":\e[K");
						fflush(stdout);

						break;
				}
				break;
			}
		}
	}

	return true;
}

int main(int argc, char** argv)
{
	int i = parse_config_or_exit(argc, argv);

	int kb_fd = get_keyboard_fd();
	if (kb_fd != -1)
	{
		int flags = 0;
		fcntl(kb_fd, F_GETFL, &flags);
		if (flags & O_NONBLOCK)
			fcntl(kb_fd, F_SETFL, flags & ~O_NONBLOCK);

		termios termios;
		tcgetattr(kb_fd, &termios);
		termios.c_lflag &= ~(ECHO | ICANON);
		tcsetattr(kb_fd, TCSANOW, &termios);
	}

	winsize ws { .ws_row = 0, .ws_col = 0 };
	if (isatty(STDOUT_FILENO))
	{
		if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
		{
			// try to make stdout fully buffered to reduce flicker
			const size_t bufsize = 2 * ws.ws_row * ws.ws_col;
			if (char* buffer = static_cast<char*>(malloc(bufsize)))
				setvbuf(stdout, buffer, _IOFBF, bufsize);
		}
	}

	if (i == argc)
	{
		if (!less_file(STDIN_FILENO, kb_fd, ws))
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	int ret = EXIT_SUCCESS;
	for (; i < argc; i++)
	{
		int fd = open(argv[i], O_RDONLY);
		if (fd == -1)
		{
			perror(argv[i]);
			continue;
		}

		if (!less_file(fd, kb_fd, ws))
			ret = EXIT_FAILURE;

		close(fd);
	}

	return ret;
}
