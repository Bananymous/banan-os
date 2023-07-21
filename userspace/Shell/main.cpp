#include <BAN/Optional.h>
#include <BAN/ScopeGuard.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define ERROR_RETURN(__msg, __ret) do { perror(__msg); return __ret; } while (false)

struct termios old_termios, new_termios;

extern char** environ;

static const char* argv0 = nullptr;

BAN::Vector<BAN::String> parse_command(BAN::StringView);

BAN::Optional<BAN::String> parse_dollar(BAN::StringView command, size_t& i)
{
	ASSERT(command[i] == '$');

	if (++i >= command.size())
		return "$"sv;
	
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

	return "$"sv;
}

BAN::Vector<BAN::String> parse_command(BAN::StringView command)
{
	enum class State
	{
		Normal,
		SingleQuote,
		DoubleQuote,
	};

	BAN::Vector<BAN::String> result;

	State state = State::Normal;
	BAN::String current;
	for (size_t i = 0; i < command.size(); i++)
	{
		char c = command[i];

		switch (state)
		{
			case State::Normal:
				if (c == '\'')
					state = State::SingleQuote;
				else if (c == '"')
					state = State::DoubleQuote;
				else if (c == '$')
				{
					auto expansion = parse_dollar(command, i);
					if (!expansion.has_value())
					{
						fprintf(stderr, "bad substitution\n");
						return {};
					}
					MUST(current.append(expansion.value()));
				}
				else if (!isspace(c))
					MUST(current.push_back(c));
				else
				{
					if (!current.empty())
					{
						MUST(result.push_back(current));
						current.clear();
					}
				}
				break;
			case State::SingleQuote:
				if (c == '\'')
					state = State::Normal;
				else
					MUST(current.push_back(c));
				break;
			case State::DoubleQuote:
				if (c == '"')
					state = State::Normal;
				else if (c != '$')
					MUST(current.push_back(c));
				else
				{
					auto expansion = parse_dollar(command, i);
					if (!expansion.has_value())
					{
						fprintf(stderr, "bad substitution\n");
						return {};
					}
					MUST(current.append(expansion.value()));
				}
				break;
		}
	}

	// FIXME: handle state != State::Normal
	MUST(result.push_back(BAN::move(current)));

	return result;
}

int execute_command(BAN::Vector<BAN::String>& args)
{
	if (args.empty())
		return 0;
	
	if (args.front() == "clear"sv)
	{
		fprintf(stdout, "\e[H\e[J");
		fflush(stdout);
		return 0;
	}
	else if (args.front() == "exit"sv)
	{
		exit(0);
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
	else if (args.front() == "env"sv)
	{
		char** current = environ;
		while (*current)
			printf("%s\n", *current++);
	}
	else if (args.front() == "kill-test"sv)
	{
		pid_t pid = fork();
		if (pid == 0)
		{
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
	}
	else if (args.front() == "cd"sv)
	{
		if (args.size() > 2)
		{
			printf("cd: too many arguments\n");
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

		int ret = execute_command(args);

		if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
			ERROR_RETURN("clock_gettime", 1);
		
		uint64_t total_ns = 0;
		total_ns += (end.tv_sec - start.tv_sec) * 1'000'000'000;
		total_ns += end.tv_nsec - start.tv_nsec;

		int secs  =  total_ns / 1'000'000'000;
		int msecs = (total_ns % 1'000'000'000) / 1'000'000;

		printf("took %d.%03d s\n", secs, msecs);

		return ret;
	}
	else
	{
		BAN::Vector<char*> cmd_args;
		MUST(cmd_args.reserve(args.size() + 1));
		for (const auto& arg : args)
			MUST(cmd_args.push_back((char*)arg.data()));
		MUST(cmd_args.push_back(nullptr));

		pid_t pid = fork();
		if (pid == 0)
		{
			execv(cmd_args.front(), cmd_args.data());
			perror("execv");
			exit(1);
		}
		if (pid == -1)
			ERROR_RETURN("fork", 1);

		int status;
		if (waitpid(pid, &status, 0) == -1)
			ERROR_RETURN("waitpid", 1);

		return status;
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
		return ""sv;

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
	fprintf(stdout, "%.*s", prompt.size(), prompt.data());
	fflush(stdout);
}

int main(int argc, char** argv)
{
	argv0 = argv[0];

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

			auto arguments = parse_command(command);
			return execute_command(arguments);
		}

		printf("unknown argument '%s'\n", argv[1]);
		return 1;
	}

	if (argc >= 1)
		setenv("SHELL", argv[0], true);
	setenv("PS1", "\e[32muser@host\e[m:\e[34m\\~\e[m$ ", false);

	tcgetattr(0, &old_termios);

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
		uint8_t ch;
		fread(&ch, 1, sizeof(char), stdin);

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
			fread(&ch, 1, sizeof(char), stdin);
			if (ch != '[')
				break;
			fread(&ch, 1, sizeof(char), stdin);
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
		case '\n':
			fputc('\n', stdout);
			if (!buffers[index].empty())
			{
				tcsetattr(0, TCSANOW, &old_termios);
				auto parsed_arguments = parse_command(buffers[index]);
				execute_command(parsed_arguments);
				tcsetattr(0, TCSANOW, &new_termios);
				MUST(history.push_back(buffers[index]));
				buffers = history;
				MUST(buffers.emplace_back(""sv));
			}
			print_prompt();
			index = buffers.size() - 1;
			col = 0;
			break;
		default:
			MUST(buffers[index].insert(ch, col++));
			fprintf(stdout, "%c\e[s%s\e[u", ch, buffers[index].data() + col);
			fflush(stdout);
			break;
		}
	}

	tcsetattr(0, TCSANOW, &old_termios);
	return 0;
}
