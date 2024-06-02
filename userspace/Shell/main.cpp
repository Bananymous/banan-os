#include <BAN/Optional.h>
#include <BAN/ScopeGuard.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define ERROR_RETURN(__msg, __ret) do { perror(__msg); return __ret; } while (false)

struct termios old_termios, new_termios;

extern char** environ;

static const char* argv0 = nullptr;
static int last_return = 0;

static BAN::String hostname;

static void clean_exit()
{
	tcsetattr(0, TCSANOW, &old_termios);
	exit(0);
}

BAN::Vector<BAN::Vector<BAN::String>> parse_command(BAN::StringView);

BAN::Optional<BAN::String> parse_dollar(BAN::StringView command, size_t& i)
{
	ASSERT(command[i] == '$');

	if (++i >= command.size())
		return "$"sv;

	if (command[i] == '?')
	{
		i++;
		return BAN::String::formatted("{}", last_return);
	}
	if (isalnum(command[i]))
	{
		size_t len = 1;
		for (; i + len < command.size(); len++)
			if (!isalnum(command[i + len]))
				break;
		BAN::String name = command.substring(i, len);
		i += len - 1;

		if (const char* value = getenv(name.data()))
			return BAN::StringView(value);
		return ""sv;
	}
	else if (command[i] == '{')
	{
		size_t len = 1;
		for (; i + len < command.size(); len++)
		{
			if (command[i + len] == '}')
				break;
			if (!isalnum(command[i + len]))
				return {};
		}

		if (i + len >= command.size())
			return {};

		BAN::String name = command.substring(i + 1, len - 1);
		i += len;

		if (const char* value = getenv(name.data()))
			return BAN::StringView(value);
		return ""sv;
	}
	else if (command[i] == '[')
	{
		return {};
	}
	else if (command[i] == '(')
	{
		size_t len = 1;
		int count = 1;
		for (; i + len < command.size(); len++)
		{
			if (command[i + len] == '(')
				count++;
			if (command[i + len] == ')')
				count--;
			if (count == 0)
				break;
		}

		if (count != 0)
			return {};

		BAN::String subcommand = command.substring(i + 1, len - 1);

		char temp[3] { '-', 'c', '\0' };
		BAN::Vector<char*> argv;
		MUST(argv.push_back((char*)argv0));
		MUST(argv.push_back(temp));
		MUST(argv.push_back((char*)subcommand.data()));
		MUST(argv.push_back(nullptr));

		int fds[2];
		if (pipe(fds) == -1)
			ERROR_RETURN("pipe", {});

		pid_t pid = fork();
		if (pid == 0)
		{
			if (dup2(fds[1], STDOUT_FILENO) == -1)
			{
				perror("dup2");
				exit(1);
			}
			close(fds[0]);
			close(fds[1]);

			execv(argv.front(), argv.data());
			perror("execv");
			exit(1);
		}
		if (pid == -1)
			ERROR_RETURN("fork", {});

		close(fds[1]);

		char buffer[100];
		BAN::String output;
		while (ssize_t ret = read(fds[0], buffer, sizeof(buffer)))
		{
			if (ret == -1)
			{
				perror("read");
				break;
			}
			MUST(output.append(BAN::StringView(buffer, ret)));
		}

		close(fds[0]);

		int status;
		if (waitpid(pid, &status, 0) == -1 && errno != ECHILD)
			ERROR_RETURN("waitpid", {});

		while (!output.empty() && output.back() == '\n')
			output.pop_back();

		i += len;
		return output;
	}

	BAN::String temp = "$"sv;
	MUST(temp.push_back(command[i]));
	return temp;
}

BAN::StringView strip_whitespace(BAN::StringView sv)
{
	size_t leading = 0;
	while (leading < sv.size() && isspace(sv[leading]))
		leading++;
	sv = sv.substring(leading);

	size_t trailing = 0;
	while (trailing < sv.size() && isspace(sv[sv.size() - trailing - 1]))
		trailing++;
	sv = sv.substring(0, sv.size() - trailing);

	return sv;
}

BAN::Vector<BAN::Vector<BAN::String>> parse_command(BAN::StringView command_view)
{
	enum class State
	{
		Normal,
		SingleQuote,
		DoubleQuote,
	};

	command_view = strip_whitespace(command_view);

	BAN::Vector<BAN::Vector<BAN::String>> result;
	BAN::Vector<BAN::String> command_args;

	State state = State::Normal;
	BAN::String current_arg;
	for (size_t i = 0; i < command_view.size(); i++)
	{
		char c = command_view[i];

		switch (state)
		{
			case State::Normal:
				if (c == '\'')
					state = State::SingleQuote;
				else if (c == '"')
					state = State::DoubleQuote;
				else if (c == '$')
				{
					auto expansion = parse_dollar(command_view, i);
					if (!expansion.has_value())
					{
						fprintf(stderr, "bad substitution\n");
						return {};
					}
					MUST(current_arg.append(expansion.value()));
				}
				else if (c == '|')
				{
					if (!current_arg.empty())
						MUST(command_args.push_back(current_arg));
					current_arg.clear();

					MUST(result.push_back(command_args));
					command_args.clear();
				}
				else if (!isspace(c))
					MUST(current_arg.push_back(c));
				else
				{
					if (!current_arg.empty())
					{
						MUST(command_args.push_back(current_arg));
						current_arg.clear();
					}
				}
				break;
			case State::SingleQuote:
				if (c == '\'')
					state = State::Normal;
				else
					MUST(current_arg.push_back(c));
				break;
			case State::DoubleQuote:
				if (c == '"')
					state = State::Normal;
				else if (c != '$')
					MUST(current_arg.push_back(c));
				else
				{
					auto expansion = parse_dollar(command_view, i);
					if (!expansion.has_value())
					{
						fprintf(stderr, "bad substitution\n");
						return {};
					}
					MUST(current_arg.append(expansion.value()));
				}
				break;
		}
	}

	// FIXME: handle state != State::Normal
	MUST(command_args.push_back(BAN::move(current_arg)));
	MUST(result.push_back(BAN::move(command_args)));

	return result;
}

int execute_command(BAN::Vector<BAN::String>& args, int fd_in, int fd_out);

int source_script(const BAN::String& path);

BAN::Optional<int> execute_builtin(BAN::Vector<BAN::String>& args, int fd_in, int fd_out)
{
	if (args.empty())
		return 0;

	FILE* fout = stdout;
	bool should_close = false;
	if (fd_out != STDOUT_FILENO)
	{
		int fd_dup = dup(fd_out);
		if (fd_dup == -1)
			ERROR_RETURN("dup", 1);
		fout = fdopen(fd_dup, "w");
		if (fout == nullptr)
			ERROR_RETURN("fdopen", 1);
		should_close = true;
	}
	BAN::ScopeGuard _([fout, should_close] { if (should_close) fclose(fout); });

	if (args.front() == "clear"sv)
	{
		fprintf(fout, "\e[H\e[2J");
		fflush(fout);
	}
	else if (args.front() == "exit"sv)
	{
		clean_exit();
	}
	else if (args.front() == "export"sv)
	{
		bool first = false;
		for (const auto& arg : args)
		{
			if (first)
			{
				first = false;
				continue;
			}

			auto split = MUST(arg.sv().split('=', true));
			if (split.size() != 2)
				continue;

			if (setenv(BAN::String(split[0]).data(), BAN::String(split[1]).data(), true) == -1)
				ERROR_RETURN("setenv", 1);
		}
	}
	else if (args.front() == "source"sv)
	{
		if (args.size() != 2)
		{
			fprintf(fout, "usage: source FILE\n");
			return 1;
		}
		return source_script(args[1]);
	}
	else if (args.front() == "env"sv)
	{
		char** current = environ;
		while (*current)
			fprintf(fout, "%s\n", *current++);
	}
	else if (args.front() == "start-gui"sv)
	{
		pid_t pid = fork();
		if (pid == 0)
			execl("/bin/WindowServer", "WindowServer", NULL);
		if (fork() == 0)
			execl("/bin/test-window", "test-window", NULL);
		if (fork() == 0)
			execl("/bin/test-window", "test-window", NULL);
		waitpid(pid, nullptr, 0);
	}
	else if (args.front() == "page-fault-test"sv)
	{
		volatile int* ptr = nullptr;
		*ptr = 0;
	}
	else if (args.front() == "kill-test"sv)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			fprintf(fout, "child\n");
			for (;;);
		}
		if (pid == -1)
		{
			perror("fork");
			return 1;
		}

		sleep(1);
		if (kill(pid, SIGSEGV) == -1)
		{
			perror("kill");
			return 1;
		}
	}
	else if (args.front() == "signal-test"sv)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
			dup2(fileno(fout), STDOUT_FILENO);
			if (signal(SIGSEGV, [](int) { printf("SIGSEGV\n"); }) == SIG_ERR)
			{
				perror("signal");
				exit(1);
			}
			printf("child\n");
			for (;;);
		}
		if (pid == -1)
		{
			perror("fork");
			return 1;
		}

		sleep(1);
		if (kill(pid, SIGSEGV) == -1)
		{
			perror("kill");
			return 1;
		}

		sleep(1);
		if (kill(pid, SIGTERM) == -1)
		{
			perror("kill");
			return 1;
		}
	}
	else if (args.front() == "printf-test"sv)
	{
		fprintf(fout, " 0.0:   %f\n", 0.0f);
		fprintf(fout, " 123.0: %f\n", 123.0f);
		fprintf(fout, " 0.123: %f\n", 0.123f);
		fprintf(fout, " NAN:   %f\n", NAN);
		fprintf(fout, "+INF:   %f\n", INFINITY);
		fprintf(fout, "-INF:   %f\n", -INFINITY);
	}
	else if (args.front() == "cd"sv)
	{
		if (args.size() > 2)
		{
			fprintf(fout, "cd: too many arguments\n");
			return 1;
		}

		BAN::StringView path;

		if (args.size() == 1)
		{
			if (const char* path_env = getenv("HOME"))
				path = path_env;
			else
				return 0;
		}
		else
			path = args[1];

		if (chdir(path.data()) == -1)
			ERROR_RETURN("chdir", 1);
	}
	else if (args.front() == "time"sv)
	{
		args.remove(0);

		timespec start, end;

		if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
			ERROR_RETURN("clock_gettime", 1);

		int ret = execute_command(args, fd_in, fd_out);

		if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
			ERROR_RETURN("clock_gettime", 1);

		uint64_t total_ns = 0;
		total_ns += (end.tv_sec - start.tv_sec) * 1'000'000'000;
		total_ns += end.tv_nsec - start.tv_nsec;

		int secs  =  total_ns / 1'000'000'000;
		int msecs = (total_ns % 1'000'000'000) / 1'000'000;

		fprintf(fout, "took %d.%03d s\n", secs, msecs);

		return ret;
	}
	else if (args.front() == "test-strtox")
	{
#define TEST(num, base) do { errno = 0; printf("strtol(\"" num "\", nullptr, " #base ") = %ld ", strtol(num, nullptr, base)); puts(errno ? strerrorname_np(errno) : ""); } while (false)
		TEST("0", 10);
		TEST("", 10);
		TEST("+", 10);
		TEST("123", 10);
		TEST("-123", 10);
		TEST("7fffffffffffffff", 10);
		TEST("7fffffffffffffff", 16);
		TEST("8000000000000000", 16);
		TEST("-8000000000000000", 16);
		TEST("-8000000000000001", 16);
		TEST("123", 0);
		TEST("0123", 0);
		TEST("0x123", 0);
		TEST("123", 1);
		TEST("hello", 10);
		TEST("hello", 36);
#undef TEST
#define TEST(num, base) do { errno = 0; printf("strtoul(\"" num "\", nullptr, " #base ") = %lu ", strtoul(num, nullptr, base)); puts(errno ? strerrorname_np(errno) : ""); } while (false)
		TEST("0", 10);
		TEST("123", 10);
		TEST("-123", 10);
		TEST("-1", 10);
		TEST("fffffffffffffff", 16);
		TEST("ffffffffffffffff", 16);
		TEST("10000000000000000", 16);
#undef TEST
#define TEST(num) do { errno = 0; printf("strtod(\"" num "\", nullptr) = %e ", strtod(num, nullptr)); puts(errno ? strerrorname_np(errno) : ""); } while (false)
		TEST("0");
		TEST(".1");
		TEST("1.");
		TEST("0x.1");
		TEST("0x1.");
		TEST("123");
		TEST("-123");
		TEST("0x123");
		TEST("123.456");
		TEST("-123.456");
		TEST("1.2e5");
		TEST("1.e5");
		TEST(".2e5");
		TEST("0x1.2p5");
		TEST("0x1.p5");
		TEST("0x.2p5");
		TEST("1e999");
		TEST("-1e999");
		TEST("1e308");
		TEST("1e-307");
		TEST("1e309");
		TEST("1e-308");
		TEST("0.00000000001e312");
		TEST("1000000000000e-312");
		TEST("0e999");
		TEST("0e-999");
		TEST("1237754.446f");
		TEST("inf");
		TEST("-inf");
		TEST("nan");
#undef TEST
	}
	else
	{
		return {};
	}

	return 0;
}

pid_t execute_command_no_wait(BAN::Vector<BAN::String>& args, int fd_in, int fd_out, pid_t pgrp)
{
	if (args.empty())
		return 0;

	BAN::Vector<char*> cmd_args;
	MUST(cmd_args.reserve(args.size() + 1));
	for (const auto& arg : args)
		MUST(cmd_args.push_back((char*)arg.data()));
	MUST(cmd_args.push_back(nullptr));

	// do PATH resolution
	BAN::String executable_file;
	if (!args.front().sv().contains('/'))
	{
		const char* path_env_cstr = getenv("PATH");
		if (path_env_cstr == nullptr)
			path_env_cstr = "";

		auto path_env_list = MUST(BAN::StringView(path_env_cstr).split(':'));
		for (auto path_env : path_env_list)
		{
			BAN::String test_file = path_env;
			MUST(test_file.push_back('/'));
			MUST(test_file.append(args.front()));

			struct stat st;
			if (stat(test_file.data(), &st) == 0)
			{
				executable_file = BAN::move(test_file);
				break;
			}
		}
	}
	else
	{
		executable_file = args.front();
	}

	// Verify that the file exists is executable
	{
		struct stat st;
		if (executable_file.empty() || stat(executable_file.data(), &st) == -1)
		{
			fprintf(stderr, "command not found: %s\n", args.front().data());
			return -1;
		}
		if ((st.st_mode & 0111) == 0)
		{
			fprintf(stderr, "permission denied: %s\n", executable_file.data());
			return -1;
		}
	}

	pid_t pid = fork();
	if (pid == 0)
	{
		if (fd_in != STDIN_FILENO)
		{
			if (dup2(fd_in, STDIN_FILENO) == -1)
			{
				perror("dup2");
				exit(1);
			}
			close(fd_in);
		}
		if (fd_out != STDOUT_FILENO)
		{
			if (dup2(fd_out, STDOUT_FILENO) == -1)
			{
				perror("dup2");
				exit(1);
			}
			close(fd_out);
		}

		if (pgrp == 0)
		{
			if(setpgid(0, 0) == -1)
			{
				perror("setpgid");
				exit(1);
			}
			if (tcsetpgrp(0, getpgrp()) == -1)
			{
				perror("tcsetpgrp");
				exit(1);
			}
		}
		else
		{
			setpgid(0, pgrp);
		}

		execv(executable_file.data(), cmd_args.data());
		perror("execv");
		exit(1);
	}

	if (pid == -1)
		ERROR_RETURN("fork", -1);

	return pid;
}

int execute_command(BAN::Vector<BAN::String>& args, int fd_in, int fd_out)
{
	pid_t pid = execute_command_no_wait(args, fd_in, fd_out, 0);
	if (pid == -1)
		return 1;

	int status;
	if (waitpid(pid, &status, 0) == -1)
		ERROR_RETURN("waitpid", 1);

	if (tcsetpgrp(0, getpgrp()) == -1)
		ERROR_RETURN("tcsetpgrp", 1);

	if (WIFSIGNALED(status))
		fprintf(stderr, "Terminated by signal %d\n", WTERMSIG(status));

	return WEXITSTATUS(status);
}

int execute_piped_commands(BAN::Vector<BAN::Vector<BAN::String>>& commands)
{
	if (commands.empty())
		return 0;

	if (commands.size() == 1)
	{
		auto& command = commands.front();
		if (auto ret = execute_builtin(command, STDIN_FILENO, STDOUT_FILENO); ret.has_value())
			return ret.value();
		return execute_command(command, STDIN_FILENO, STDOUT_FILENO);
	}

	BAN::Vector<int> exit_codes(commands.size(), 0);
	BAN::Vector<pid_t> processes(commands.size(), -1);
	pid_t pgrp = 0;

	int next_stdin = STDIN_FILENO;
	for (size_t i = 0; i < commands.size(); i++)
	{
		bool last  = (i == commands.size() - 1);

		int pipefd[2] { -1, STDOUT_FILENO };
		if (!last && pipe(pipefd) == -1)
		{
			if (i > 0)
				close(next_stdin);
			perror("pipe");
			break;
		}

		auto builtin_ret = execute_builtin(commands[i], next_stdin, pipefd[1]);
		if (builtin_ret.has_value())
			exit_codes[i] = builtin_ret.value();
		else
		{
			pid_t pid = execute_command_no_wait(commands[i], next_stdin, pipefd[1], pgrp);
			processes[i] = pid;
			if (pgrp == 0)
				pgrp = pid;
		}

		if (next_stdin != STDIN_FILENO)
			close(next_stdin);
		if (pipefd[1] != STDOUT_FILENO)
			close(pipefd[1]);
		next_stdin = pipefd[0];
	}

	for (size_t i = 0; i < commands.size(); i++)
	{
		if (processes[i] == -1)
			continue;

		int status;
		if (waitpid(processes[i], &status, 0) == -1)
		{
			perror("waitpid");
			exit_codes[i] = 69420;
			continue;
		}

		if (WIFSIGNALED(status))
			fprintf(stderr, "Terminated by signal %d\n", WTERMSIG(status));

		if (WEXITSTATUS(status))
			exit_codes[i] = WEXITSTATUS(status);
	}

	if (tcsetpgrp(0, getpgrp()) == -1)
		ERROR_RETURN("tcsetpgrp", 1);

	return exit_codes.back();
}

int parse_and_execute_command(BAN::StringView command)
{
	if (command.empty())
		return 0;
	auto parsed_commands = parse_command(command);
	if (parsed_commands.empty())
		return 0;
	tcsetattr(0, TCSANOW, &old_termios);
	int ret = execute_piped_commands(parsed_commands);
	tcsetattr(0, TCSANOW, &new_termios);
	return ret;
}

int source_script(const BAN::String& path)
{
	FILE* fp = fopen(path.data(), "r");
	if (fp == nullptr)
		ERROR_RETURN("fopen", 1);

	int ret = 0;

	BAN::String command;
	char temp_buffer[128];
	while (fgets(temp_buffer, sizeof(temp_buffer), fp))
	{
		MUST(command.append(temp_buffer));
		if (command.back() != '\n')
			continue;

		command.pop_back();

		if (!command.empty())
			if (int temp = parse_and_execute_command(command))
				ret = temp;
		command.clear();
	}

	if (!command.empty())
		if (int temp = parse_and_execute_command(command))
			ret = temp;

	fclose(fp);

	return ret;
}

bool exists(const BAN::String& path)
{
	struct stat st;
	return stat(path.data(), &st) == 0;
}

int source_shellrc()
{
	if (char* home = getenv("HOME"))
	{
		BAN::String path(home);
		MUST(path.append("/.shellrc"sv));
		if (exists(path))
			return source_script(path);
	}
	return 0;
}

int character_length(BAN::StringView prompt)
{
	int length { 0 };
	bool in_escape { false };
	for (char c : prompt)
	{
		if (in_escape)
		{
			if (isalpha(c))
				in_escape = false;
		}
		else
		{
			if (c == '\e')
				in_escape = true;
			else if (((uint8_t)c & 0xC0) != 0x80)
				length++;
		}
	}
	return length;
}

BAN::String get_prompt()
{
	const char* raw_prompt = getenv("PS1");
	if (raw_prompt == nullptr)
		return "$ "sv;

	BAN::String prompt;
	for (int i = 0; raw_prompt[i]; i++)
	{
		char ch = raw_prompt[i];
		if (ch == '\\')
		{
			switch (raw_prompt[++i])
			{
			case 'e':
				MUST(prompt.push_back('\e'));
				break;
			case 'n':
				MUST(prompt.push_back('\n'));
				break;
			case '\\':
				MUST(prompt.push_back('\\'));
				break;
			case '~':
			{
				char buffer[256];
				if (getcwd(buffer, sizeof(buffer)) == nullptr)
					strcpy(buffer, strerrorname_np(errno));

				const char* home = getenv("HOME");
				size_t home_len = home ? strlen(home) : 0;
				if (home && strncmp(buffer, home, home_len) == 0)
				{
					MUST(prompt.push_back('~'));
					MUST(prompt.append(buffer + home_len));
				}
				else
				{
					MUST(prompt.append(buffer));
				}

				break;
			}
			case 'u':
			{
				static char* username = nullptr;
				if (username == nullptr)
				{
					auto* passwd = getpwuid(geteuid());
					if (passwd == nullptr)
						break;
					username = new char[strlen(passwd->pw_name) + 1];
					strcpy(username, passwd->pw_name);
					endpwent();
				}
				MUST(prompt.append(username));
				break;
			}
			case 'h':
			{
				MUST(prompt.append(hostname));
				break;
			}
			case '\0':
				MUST(prompt.push_back('\\'));
				break;
			default:
				MUST(prompt.push_back('\\'));
				MUST(prompt.push_back(*raw_prompt));
				break;
			}
		}
		else
		{
			MUST(prompt.push_back(ch));
		}
	}

	return prompt;
}

int prompt_length()
{
	return character_length(get_prompt());
}

void print_prompt()
{
	auto prompt = get_prompt();
	fprintf(stdout, "%.*s", (int)prompt.size(), prompt.data());
	fflush(stdout);
}

int main(int argc, char** argv)
{
	argv0 = argv[0];

	if (signal(SIGINT, [](int) {}) == SIG_ERR)
		perror("signal");

	tcgetattr(0, &old_termios);

	{
		FILE* fp = fopen("/etc/hostname", "r");
		if (fp != NULL)
		{
			char buffer[512];
			while (size_t nbyte = fread(buffer, 1, sizeof(buffer), fp))
			{
				if (nbyte == 0)
					break;
				MUST(hostname.append(BAN::StringView(buffer, nbyte)));
			}
			fclose(fp);
		}
		if (!hostname.empty() && hostname.back() == '\n')
			hostname.pop_back();
	}

	if (argc >= 2)
	{
		if (strcmp(argv[1], "-c") == 0)
		{
			if (argc == 2)
			{
				printf("-c requires an argument\n");
				return 1;
			}

			BAN::String command;
			MUST(command.append(argv[2]));

			auto commands = parse_command(command);
			return execute_piped_commands(commands);
		}

		printf("unknown argument '%s'\n", argv[1]);
		return 1;
	}

	if (argc >= 1)
		setenv("SHELL", argv[0], true);

	source_shellrc();

	new_termios = old_termios;
	new_termios.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(0, TCSANOW, &new_termios);

	BAN::Vector<BAN::String> buffers, history;
	MUST(buffers.emplace_back(""sv));
	size_t index = 0;
	size_t col = 0;

	int waiting_utf8 = 0;

	print_prompt();

	while (true)
	{
		int chi = getchar();
		if (chi == EOF)
		{
			if (errno == EINTR)
			{
				clearerr(stdin);
				continue;
			}
			perror("getchar");
			return 1;
		}

		uint8_t ch = chi;

		if (waiting_utf8 > 0)
		{
			waiting_utf8--;

			ASSERT((ch & 0xC0) == 0x80);

			fputc(ch, stdout);
			MUST(buffers[index].insert(ch, col++));
			if (waiting_utf8 == 0)
			{
				fprintf(stdout, "\e[s%s\e[u", buffers[index].data() + col);
				fflush(stdout);
			}
			continue;
		}
		else if (ch & 0x80)
		{
			if ((ch & 0xE0) == 0xC0)
				waiting_utf8 = 1;
			else if ((ch & 0xF0) == 0xE0)
				waiting_utf8 = 2;
			else if ((ch & 0xF8) == 0xF0)
				waiting_utf8 = 3;
			else
				ASSERT_NOT_REACHED();

			fputc(ch, stdout);
			MUST(buffers[index].insert(ch, col++));
			continue;
		}

		switch (ch)
		{
		case '\e':
			ch = getchar();
			if (ch != '[')
				break;
			ch = getchar();
			switch (ch)
			{
				case 'A': if (index > 0)					{ index--; col = buffers[index].size(); fprintf(stdout, "\e[%dG%s\e[K", prompt_length() + 1, buffers[index].data()); fflush(stdout); } break;
				case 'B': if (index < buffers.size() - 1)	{ index++; col = buffers[index].size(); fprintf(stdout, "\e[%dG%s\e[K", prompt_length() + 1, buffers[index].data()); fflush(stdout); } break;
				case 'C': if (col < buffers[index].size())	{ col++; while ((buffers[index][col - 1] & 0xC0) == 0x80) col++; fprintf(stdout, "\e[C"); fflush(stdout); } break;
				case 'D': if (col > 0)						{ while ((buffers[index][col - 1] & 0xC0) == 0x80) col--; col--; fprintf(stdout, "\e[D"); fflush(stdout); } break;
			}
			break;
		case '\x0C': // ^L
		{
			int x = prompt_length() + character_length(buffers[index].sv().substring(col)) + 1;
			fprintf(stdout, "\e[H\e[J");
			print_prompt();
			fprintf(stdout, "%s\e[u\e[1;%dH", buffers[index].data(), x);
			fflush(stdout);
			break;
		}
		case '\b':
			if (col > 0)
			{
				while ((buffers[index][col - 1] & 0xC0) == 0x80)
					buffers[index].remove(--col);
				buffers[index].remove(--col);
				fprintf(stdout, "\b\e[s%s \e[u", buffers[index].data() + col);
				fflush(stdout);
			}
			break;
		case '\x01': // ^A
			col = 0;
			fprintf(stdout, "\e[%dG", prompt_length() + 1);
			fflush(stdout);
			break;
		case '\x03': // ^C
			fputc('\n', stdout);
			print_prompt();
			buffers[index].clear();
			col = 0;
			break;
		case '\x04':
			fprintf(stdout, "\n");
			clean_exit();
			break;
		case '\n':
			fputc('\n', stdout);
			if (!buffers[index].empty())
			{
				last_return = parse_and_execute_command(buffers[index]);
				MUST(history.push_back(buffers[index]));
				buffers = history;
				MUST(buffers.emplace_back(""sv));
			}
			print_prompt();
			index = buffers.size() - 1;
			col = 0;
			break;
		case '\t':
			// FIXME: Implement tab completion or something
			break;
		default:
			MUST(buffers[index].insert(ch, col++));
			fprintf(stdout, "%c\e[s%s\e[u", ch, buffers[index].data() + col);
			fflush(stdout);
			break;
		}
	}

	clean_exit();
}
