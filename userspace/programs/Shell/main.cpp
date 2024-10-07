#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/Optional.h>
#include <BAN/Sort.h>
#include <BAN/String.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define ERROR_RETURN(__msg, __ret) do { perror(__msg); return __ret; } while (false)

extern char** environ;

static struct termios old_termios, new_termios;

static char s_shell_path[PATH_MAX];
static int last_return = 0;

static BAN::String hostname;

struct SingleCommand
{
	BAN::Vector<BAN::String> arguments;
};

struct PipedCommand
{
	BAN::Vector<SingleCommand> commands;
};

struct CommandList
{
	enum class Condition
	{
		Always,
		OnSuccess,
		OnFailure,
	};

	struct Command
	{
		BAN::String expression;
		Condition condition;
	};
	BAN::Vector<Command> commands;
};

struct BuiltinCommand
{
	int (*function)(const SingleCommand&, FILE* fout, int fd_in, int fd_out);
};
static BAN::HashMap<BAN::String, BuiltinCommand> s_builtin_commands;

static BAN::HashMap<BAN::String, BAN::Vector<BAN::String>> s_aliases;

static BAN::StringView strip_whitespace(BAN::StringView sv)
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

static BAN::Optional<BAN::String> parse_dollar(BAN::StringView command, size_t& i)
{
	ASSERT(command[i] == '$');

	if (++i >= command.size())
		return BAN::String("$"_sv);

	if (command[i] == '?')
	{
		i++;
		return MUST(BAN::String::formatted("{}", last_return));
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
			return BAN::String(value);
		return BAN::String();
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
			return BAN::String(value);
		return BAN::String();
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
		MUST(argv.push_back(s_shell_path));
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
		if (waitpid(pid, &status, 0) == -1)
			ERROR_RETURN("waitpid", {});

		while (!output.empty() && output.back() == '\n')
			output.pop_back();

		i += len;
		return output;
	}

	BAN::String temp = "$"_sv;
	MUST(temp.push_back(command[i]));
	return temp;
}

static SingleCommand parse_single_command(BAN::StringView command_view, bool parse_aliases = true)
{
	constexpr auto can_escape =
		[](char c)
		{
			switch (c)
			{
				case 'e':
				case 'n':
				case 't':
				case 'r':
				case '"':
				case '\'':
				case ' ':
					return true;
			}
			return false;
		};

	constexpr auto parse_escaped =
		[](char c) -> char
		{
			switch (c)
			{
				case 'e':  return '\e';
				case 'n':  return '\n';
				case 't':  return '\t';
				case 'r':  return '\r';
				case '"':  return '"';
				case '\'': return '\'';
				case ' ':  return ' ';
			}
			ASSERT_NOT_REACHED();
		};

	while (!command_view.empty() && isspace(command_view.front()))
		command_view = command_view.substring(1);
	while (!command_view.empty() && isspace(command_view.back()))
		command_view = command_view.substring(0, command_view.size() - 1);

	SingleCommand result;
	BAN::String current_argument;
	for (size_t i = 0; i < command_view.size(); i++)
	{
		const char current = command_view[i];

		if (isspace(current))
		{
			MUST(result.arguments.push_back(BAN::move(current_argument)));
			current_argument.clear();
			while (i + 1 < command_view.size() && isspace(command_view[i + 1]))
				i++;
			continue;
		}

		switch (current)
		{
			case '\\':
				if (i + 1 < command_view.size() && can_escape(command_view[i + 1]))
					MUST(current_argument.push_back(parse_escaped(command_view[++i])));
				else
					MUST(current_argument.push_back('\\'));
				break;
			case '$':
				if (auto expansion = parse_dollar(command_view, i); expansion.has_value())
					MUST(current_argument.append(expansion.release_value()));
				else
				{
					fprintf(stderr, "bad substitution\n");
					return {};
				}
				break;
			case '~':
				if (i == 0 || (isspace(command_view[i - 1]) && (i == 1 || command_view[i - 2] != '\\')))
				{
					const char* home_env = getenv("HOME");
					if (home_env)
					{
						MUST(current_argument.append(home_env));
						break;
					}
				}
				MUST(current_argument.push_back('~'));
				break;
			case '\'':
				while (++i < command_view.size())
				{
					if (command_view[i] == current)
						break;
					if (command_view[i] == '\\' && i + 1 < command_view.size() && can_escape(command_view[i + 1]))
						MUST(current_argument.push_back(parse_escaped(command_view[++i])));
					else
						MUST(current_argument.push_back(command_view[i]));
				}
				break;
			case '"':
				while (++i < command_view.size())
				{
					if (command_view[i] == current)
						break;
					if (command_view[i] == '\\' && i + 1 < command_view.size() && can_escape(command_view[i + 1]))
						MUST(current_argument.push_back(parse_escaped(command_view[++i])));
					else if (!(current == '"' && command_view[i] == '$'))
						MUST(current_argument.push_back(command_view[i]));
					else
					{
						if (auto expansion = parse_dollar(command_view, i); expansion.has_value())
							MUST(current_argument.append(expansion.release_value()));
						else
						{
							fprintf(stderr, "bad substitution\n");
							return {};
						}
					}
				}
				break;
			default:
				MUST(current_argument.push_back(command_view[i]));
				break;
		}
	}

	MUST(result.arguments.push_back(BAN::move(current_argument)));

	if (parse_aliases)
	{
		BAN::HashSet<BAN::String> matched_aliases;
		while (!result.arguments.empty() && !matched_aliases.contains(result.arguments.front()))
		{
			auto it = s_aliases.find(result.arguments.front());
			if (it == s_aliases.end())
				break;
			MUST(matched_aliases.insert(result.arguments.front()));
			result.arguments.remove(0);
			for (size_t i = 0; i < it->value.size(); i++)
				MUST(result.arguments.insert(i, it->value[i]));
		}
	}

	return BAN::move(result);
}

static PipedCommand parse_piped_command(BAN::StringView command_view)
{
	PipedCommand result;

	for (size_t i = 0; i < command_view.size(); i++)
	{
		const char current = command_view[i];
		switch (current)
		{
			case '\\':
				i++;
				break;
			case '\'':
			case '"':
				while (++i < command_view.size())
				{
					if (command_view[i] == current)
						break;
					if (command_view[i] == '\\')
						i++;
				}
				break;
			case '|':
				MUST(result.commands.emplace_back(parse_single_command(command_view.substring(0, i))));
				command_view = command_view.substring(i + 1);
				i = -1;
				break;
		}
	}

	MUST(result.commands.emplace_back(parse_single_command(command_view)));

	return BAN::move(result);
}

static CommandList parse_command_list(BAN::StringView command_view)
{
	CommandList result;
	CommandList::Condition next_condition = CommandList::Condition::Always;
	for (size_t i = 0; i < command_view.size(); i++)
	{
		const char current = command_view[i];
		switch (current)
		{
			case '\\':
				i++;
				break;
			case '\'':
			case '"':
				while (++i < command_view.size())
				{
					if (command_view[i] == '\\')
						i++;
					else if (command_view[i] == current)
						break;
				}
				break;
			case ';':
				MUST(result.commands.emplace_back(
					strip_whitespace(command_view.substring(0, i)),
					next_condition
				));
				command_view = strip_whitespace(command_view.substring(i + 1));
				next_condition = CommandList::Condition::Always;
				i = -1;
				break;
			case '|':
			case '&':
				if (i + 1 >= command_view.size() || command_view[i + 1] != current)
					break;
				MUST(result.commands.emplace_back(
					strip_whitespace(command_view.substring(0, i)),
					next_condition
				));
				command_view = strip_whitespace(command_view.substring(i + 2));
				next_condition = (current == '|') ? CommandList::Condition::OnFailure : CommandList::Condition::OnSuccess;
				i = -1;
				break;
		}
	}

	MUST(result.commands.emplace_back(
		strip_whitespace(command_view),
		next_condition
	));

	for (const auto& [expression, _] : result.commands)
	{
		if (!expression.empty())
			continue;
		fprintf(stderr, "expected an expression\n");
		return {};
	}

	return BAN::move(result);
}

static int execute_command(const SingleCommand& command, int fd_in, int fd_out);

static int source_script(const BAN::String& path);

static BAN::Optional<int> execute_builtin(const SingleCommand& command, int fd_in, int fd_out)
{
	if (command.arguments.empty())
		return 0;

	auto it = s_builtin_commands.find(command.arguments.front());
	if (it == s_builtin_commands.end())
		return {};

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

	int ret = it->value.function(command, fout, fd_in, fd_out);

	if (should_close)
		fclose(fout);

	return ret;
}

static void install_builtin_commands()
{
	MUST(s_builtin_commands.emplace("clear"_sv,
		[](const SingleCommand&, FILE* fout, int, int) -> int
		{
			fprintf(fout, "\e[H\e[3J");
			fflush(fout);
			return 0;
		}
	));

	MUST(s_builtin_commands.emplace("exit"_sv,
		[](const SingleCommand& command, FILE*, int, int) -> int
		{
			int exit_code = 0;
			if (command.arguments.size() > 1)
			{
				auto exit_string = command.arguments[1].sv();
				for (size_t i = 0; i < exit_string.size() && isdigit(exit_string[i]); i++)
					exit_code = (exit_code * 10) + (exit_string[i] - '0');
			}
			exit(exit_code);
			ASSERT_NOT_REACHED();
		}
	));

	MUST(s_builtin_commands.emplace("export"_sv,
		[](const SingleCommand& command, FILE*, int, int) -> int
		{
			bool first = false;
			for (const auto& argument : command.arguments)
			{
				if (first)
				{
					first = false;
					continue;
				}

				auto split = MUST(argument.sv().split('=', true));
				if (split.size() != 2)
					continue;

				if (setenv(BAN::String(split[0]).data(), BAN::String(split[1]).data(), true) == -1)
					ERROR_RETURN("setenv", 1);
			}
			return 0;
		}
	));

	MUST(s_builtin_commands.emplace("alias"_sv,
		[](const SingleCommand& command, FILE* fout, int, int) -> int
		{
			const auto print_alias =
				[fout](const BAN::String& alias, const BAN::Vector<BAN::String>& value)
				{
					fprintf(fout, "%s='", alias.data());
					for (size_t i = 0; i < value.size(); i++)
					{
						if (i != 0)
							fprintf(fout, " ");
						fprintf(fout, "%s", value[i].data());
					}
					fprintf(fout, "'\n");
				};

			if (command.arguments.size() == 1)
			{
				for (const auto& [alias, value] : s_aliases)
					print_alias(alias, value);
				return 0;
			}

			for (size_t i = 1; i < command.arguments.size(); i++)
			{
				auto idx = command.arguments[i].sv().find('=');
				if (idx.has_value() && idx.value() == 0)
					continue;
				if (!idx.has_value())
				{
					auto it = s_aliases.find(command.arguments[i]);
					if (it != s_aliases.end())
						print_alias(command.arguments[i], it->value);
				}
				else
				{
					auto alias = command.arguments[i].sv().substring(0, idx.value());
					auto value = command.arguments[i].sv().substring(idx.value() + 1);
					auto parsed_alias = parse_single_command(value, false);

					if (s_aliases.contains(alias))
						s_aliases.remove(alias);
					MUST(s_aliases.insert(alias, BAN::move(parsed_alias.arguments)));
				}
			}

			return 0;
		}
	));

	MUST(s_builtin_commands.emplace("source"_sv,
		[](const SingleCommand& command, FILE* fout, int, int) -> int
		{
			if (command.arguments.size() != 2)
			{
				fprintf(fout, "usage: source FILE\n");
				return 1;
			}
			return source_script(command.arguments[1]);
		}
	));

	MUST(s_builtin_commands.emplace("env"_sv,
		[](const SingleCommand&, FILE* fout, int, int) -> int
		{
			char** current = environ;
			while (current && *current)
				fprintf(fout, "%s\n", *current++);
			return 0;
		}
	));

	MUST(s_builtin_commands.emplace("cd"_sv,
		[](const SingleCommand& command, FILE* fout, int, int) -> int
		{
			if (command.arguments.size() > 2)
			{
				fprintf(fout, "cd: too many arguments\n");
				return 1;
			}

			BAN::StringView path;

			if (command.arguments.size() == 1)
			{
				if (const char* path_env = getenv("HOME"))
					path = path_env;
				else
					return 0;
			}
			else
				path = command.arguments[1];

			if (chdir(path.data()) == -1)
				ERROR_RETURN("chdir", 1);

			return 0;
		}
	));

	MUST(s_builtin_commands.emplace("time"_sv,
		[](const SingleCommand& command, FILE* fout, int fd_in, int fd_out) -> int
		{
			SingleCommand timed_command;
			MUST(timed_command.arguments.reserve(command.arguments.size() - 1));
			for (size_t i = 1; i < command.arguments.size(); i++)
				MUST(timed_command.arguments.emplace_back(command.arguments[i]));

			timespec start, end;

			if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
				ERROR_RETURN("clock_gettime", 1);

			int ret = execute_command(timed_command, fd_in, fd_out);

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
	));

	MUST(s_builtin_commands.emplace("start-gui"_sv,
		[](const SingleCommand&, FILE*, int, int) -> int
		{
			const pid_t pid = fork();
			if (pid == -1)
				return 1;
			if (pid == 0)
				execl("/bin/WindowServer", "WindowServer", NULL);
			if (fork() == 0)
				execl("/bin/Terminal", "Terminal", NULL);
			waitpid(pid, nullptr, 0);
			return 0;
		}
	));
}

static pid_t execute_command_no_wait(const SingleCommand& command, int fd_in, int fd_out, pid_t pgrp)
{
	ASSERT(!command.arguments.empty());

	BAN::Vector<char*> cmd_args;
	MUST(cmd_args.reserve(command.arguments.size() + 1));
	for (const auto& arg : command.arguments)
		MUST(cmd_args.push_back((char*)arg.data()));
	MUST(cmd_args.push_back(nullptr));

	// do PATH resolution
	BAN::String executable_file;
	if (!command.arguments.front().sv().contains('/'))
	{
		const char* path_env_cstr = getenv("PATH");
		if (path_env_cstr == nullptr)
			path_env_cstr = "";

		auto path_env_list = MUST(BAN::StringView(path_env_cstr).split(':'));
		for (auto path_env : path_env_list)
		{
			BAN::String test_file = path_env;
			MUST(test_file.push_back('/'));
			MUST(test_file.append(command.arguments.front()));

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
		executable_file = command.arguments.front();
	}

	// Verify that the file exists is executable
	{
		struct stat st;
		if (executable_file.empty() || stat(executable_file.data(), &st) == -1)
		{
			fprintf(stderr, "command not found: %s\n", command.arguments.front().data());
			return -1;
		}
		if ((st.st_mode & 0111) == 0)
		{
			fprintf(stderr, "permission denied: %s\n", executable_file.data());
			return -1;
		}
	}

	const pid_t pid = fork();
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

		execv(executable_file.data(), cmd_args.data());
		perror("execv");
		exit(1);
	}

	if (pid == -1)
		ERROR_RETURN("fork", -1);

	if (pgrp == 0 && isatty(0))
	{
		if(setpgid(pid, pid) == -1)
			perror("setpgid");
		if (tcsetpgrp(0, pid) == -1)
			perror("tcsetpgrp");
	}
	else
	{
		setpgid(pid, pgrp);
	}

	return pid;
}

static int execute_command(const SingleCommand& command, int fd_in, int fd_out)
{
	const pid_t pid = execute_command_no_wait(command, fd_in, fd_out, 0);
	if (pid == -1)
		return 1;

	int status;
	if (waitpid(pid, &status, 0) == -1)
		ERROR_RETURN("waitpid", 1);

	if (isatty(0) && tcsetpgrp(0, getpgrp()) == -1)
		ERROR_RETURN("tcsetpgrp", 1);

	if (WIFSIGNALED(status))
		fprintf(stderr, "Terminated by signal %d\n", WTERMSIG(status));

	return WEXITSTATUS(status);
}

static int execute_piped_commands(const PipedCommand& piped_command)
{
	if (piped_command.commands.empty())
		return 0;

	if (piped_command.commands.size() == 1)
	{
		auto& command = piped_command.commands.front();
		if (auto ret = execute_builtin(command, STDIN_FILENO, STDOUT_FILENO); ret.has_value())
			return ret.value();
		return execute_command(command, STDIN_FILENO, STDOUT_FILENO);
	}

	BAN::Vector<int> exit_codes(piped_command.commands.size(), 0);
	BAN::Vector<pid_t> processes(piped_command.commands.size(), -1);
	pid_t pgrp = 0;

	int next_stdin = STDIN_FILENO;
	for (size_t i = 0; i < piped_command.commands.size(); i++)
	{
		const bool last = (i == piped_command.commands.size() - 1);

		int pipefd[2] { -1, STDOUT_FILENO };
		if (!last && pipe(pipefd) == -1)
		{
			if (i > 0)
				close(next_stdin);
			perror("pipe");
			break;
		}

		auto builtin_ret = execute_builtin(piped_command.commands[i], next_stdin, pipefd[1]);
		if (builtin_ret.has_value())
			exit_codes[i] = builtin_ret.value();
		else
		{
			pid_t pid = execute_command_no_wait(piped_command.commands[i], next_stdin, pipefd[1], pgrp);
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

	for (size_t i = 0; i < piped_command.commands.size(); i++)
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

	if (isatty(0) && tcsetpgrp(0, getpgrp()) == -1)
		ERROR_RETURN("tcsetpgrp", 1);

	return exit_codes.back();
}

static int parse_and_execute_command(BAN::StringView command)
{
	command = strip_whitespace(command);
	if (command.empty())
		return 0;

	auto command_list = parse_command_list(command);
	if (command_list.commands.empty())
		return 0;

	tcsetattr(0, TCSANOW, &old_termios);

	last_return = 0;
	for (const auto& [expression, condition] : command_list.commands)
	{
		bool should_run = false;
		switch (condition)
		{
			case CommandList::Condition::Always:
				should_run = true;
				break;
			case CommandList::Condition::OnSuccess:
				should_run = (last_return == 0);
				break;
			case CommandList::Condition::OnFailure:
				should_run = (last_return != 0);
				break;
		}

		if (!should_run)
			continue;

		last_return = execute_piped_commands(parse_piped_command(expression));
	}

	tcsetattr(0, TCSANOW, &new_termios);

	return last_return;
}

static int source_script(const BAN::String& path)
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

static bool exists(const BAN::String& path)
{
	struct stat st;
	return stat(path.data(), &st) == 0;
}

static int source_shellrc()
{
	if (char* home = getenv("HOME"))
	{
		BAN::String path(home);
		MUST(path.append("/.shellrc"_sv));
		if (exists(path))
			return source_script(path);
	}
	return 0;
}

static BAN::Vector<BAN::String> list_matching_entries(BAN::StringView path, BAN::StringView start, bool require_executable)
{
	ASSERT(path.size() < PATH_MAX);

	char path_cstr[PATH_MAX];
	memcpy(path_cstr, path.data(), path.size());
	path_cstr[path.size()] = '\0';

	DIR* dirp = opendir(path_cstr);
	if (dirp == nullptr)
		return {};

	BAN::Vector<BAN::String> result;

	dirent* entry;
	while ((entry = readdir(dirp)))
	{
		if (entry->d_name[0] == '.' && !start.starts_with("."_sv))
			continue;
		if (strncmp(entry->d_name, start.data(), start.size()))
			continue;

		struct stat st;
		if (fstatat(dirfd(dirp), entry->d_name, &st, 0))
			continue;

		if (require_executable)
		{
			if (S_ISDIR(st.st_mode))
				continue;
			if (!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXUSR)))
				continue;
		}

		MUST(result.emplace_back(entry->d_name + start.size()));
		if (S_ISDIR(st.st_mode))
			MUST(result.back().push_back('/'));
	}

	closedir(dirp);

	return BAN::move(result);
}

struct TabCompletion
{
	bool should_escape_spaces;
	BAN::StringView prefix;
	BAN::Vector<BAN::String> completions;
};

static TabCompletion list_tab_completion_entries(BAN::StringView command)
{
	enum class CompletionType
	{
		Command,
		File,
	};

	BAN::StringView prefix = command;
	BAN::String last_argument;
	CompletionType completion_type = CompletionType::Command;

	bool should_escape_spaces = true;
	for (size_t i = 0; i < command.size(); i++)
	{
		if (command[i] == '\\')
		{
			i++;
			if (i < command.size())
				MUST(last_argument.push_back(command[i]));
		}
		else if (isspace(command[i]) || command[i] == ';' || command[i] == '|' || command.substring(i).starts_with("&&"_sv))
		{
			if (!isspace(command[i]))
				completion_type = CompletionType::Command;
			else if (!last_argument.empty())
				completion_type = CompletionType::File;
			if (auto rest = command.substring(i); rest.starts_with("||"_sv) || rest.starts_with("&&"_sv))
				i++;
			prefix = command.substring(i + 1);
			last_argument.clear();
			should_escape_spaces = true;
		}
		else if (command[i] == '\'' || command[i] == '"')
		{
			const char quote_type = command[i++];
			while (i < command.size() && command[i] != quote_type)
				MUST(last_argument.push_back(command[i++]));
			should_escape_spaces = false;
		}
		else
		{
			MUST(last_argument.push_back(command[i]));
		}
	}

	if (last_argument.sv().contains('/'))
		completion_type = CompletionType::File;

	BAN::Vector<BAN::String> result;
	switch (completion_type)
	{
		case CompletionType::Command:
		{
			const char* path_env = getenv("PATH");
			if (path_env)
			{
				auto splitted_path_env = MUST(BAN::StringView(path_env).split(':'));
				for (auto path : splitted_path_env)
				{
					auto matching_entries = list_matching_entries(path, last_argument, true);
					MUST(result.reserve(result.size() + matching_entries.size()));
					for (auto&& entry : matching_entries)
						MUST(result.push_back(BAN::move(entry)));
				}
			}

			for (const auto& [builtin_name, _] : s_builtin_commands)
			{
				if (!builtin_name.sv().starts_with(last_argument))
					continue;
				MUST(result.emplace_back(builtin_name.sv().substring(last_argument.size())));
			}

			for (const auto& [alias_name, _] : s_aliases)
			{
				if (!alias_name.sv().starts_with(last_argument))
					continue;
				MUST(result.emplace_back(alias_name.sv().substring(last_argument.size())));
			}

			break;
		}
		case CompletionType::File:
		{
			BAN::String dir_path;
			if (last_argument.sv().starts_with("/"_sv))
				MUST(dir_path.push_back('/'));
			else
			{
				char cwd_buffer[PATH_MAX];
				if (getcwd(cwd_buffer, sizeof(cwd_buffer)) == nullptr)
					return {};
				MUST(dir_path.reserve(strlen(cwd_buffer) + 1));
				MUST(dir_path.append(cwd_buffer));
				MUST(dir_path.push_back('/'));
			}

			auto match_against = last_argument.sv();
			if (auto idx = match_against.rfind('/'); idx.has_value())
			{
				MUST(dir_path.append(match_against.substring(0, idx.value())));
				match_against = match_against.substring(idx.value() + 1);
			}

			result = list_matching_entries(dir_path, match_against, false);

			break;
		}
	}

	if (auto idx = prefix.rfind('/'); idx.has_value())
		prefix = prefix.substring(idx.value() + 1);

	return { should_escape_spaces, prefix, BAN::move(result) };
}

static int character_length(BAN::StringView prompt)
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

static BAN::String get_prompt()
{
	const char* raw_prompt = getenv("PS1");
	if (raw_prompt == nullptr)
		return "$ "_sv;

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

static int prompt_length()
{
	return character_length(get_prompt());
}

static void print_prompt()
{
	auto prompt = get_prompt();
	printf("%.*s", (int)prompt.size(), prompt.data());
	fflush(stdout);
}

int main(int argc, char** argv)
{
	realpath(argv[0], s_shell_path);

	struct sigaction sa;
	sa.sa_flags = 0;

	sa.sa_handler = [](int) {};
	sigaction(SIGINT, &sa, nullptr);

	sa.sa_handler = SIG_IGN;
	sigaction(SIGTTOU, &sa, nullptr);

	tcgetattr(0, &old_termios);

	char hostname_buffer[HOST_NAME_MAX];
	if (gethostname(hostname_buffer, sizeof(hostname_buffer)) == 0) {
		MUST(hostname.append(hostname_buffer));
	}

	new_termios = old_termios;
	new_termios.c_lflag &= ~(ECHO | ICANON);
	tcsetattr(0, TCSANOW, &new_termios);

	atexit([]() { tcsetattr(0, TCSANOW, &old_termios); });

	install_builtin_commands();

	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-')
			return source_script(BAN::String(argv[i]));

		if (strcmp(argv[i], "-c") == 0)
		{
			if (i + 1 >= argc)
			{
				printf("-c requires an argument\n");
				return 1;
			}
			return parse_and_execute_command(BAN::String(argv[i + 1]));
		}
		else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0)
		{
			printf("banan-sh 1.0\n");
			return 0;
		}
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			printf("usage: %s [options...]\n", argv[0]);
			printf("  -c             run following argument as an argument\n");
			printf("  -v, --version  print version information and exit\n");
			printf("  -h, --help     print this message and exit\n");
			return 0;
		}
		else
		{
			printf("unknown argument '%s'\n", argv[i]);
			return 1;
		}
	}

	source_shellrc();

	BAN::Vector<BAN::String> buffers, history;
	MUST(buffers.emplace_back(""_sv));
	size_t index = 0;
	size_t col = 0;

	BAN::Optional<ssize_t> tab_index;
	BAN::Optional<BAN::Vector<BAN::String>> tab_completions;
	size_t tab_completion_keep = 0;

	int waiting_utf8 = 0;

	print_prompt();

	while (true)
	{
		int chi = getchar();
		if (chi == EOF)
		{
			if (errno != EINTR)
			{
				perror("getchar");
				return 1;
			}

			clearerr(stdin);
			buffers = history;
			MUST(buffers.emplace_back(""_sv));
			col = 0;
			putchar('\n');
			print_prompt();
			continue;
		}

		uint8_t ch = chi;
		if (ch != '\t')
		{
			tab_completions.clear();
			tab_index.clear();
		}

		if (waiting_utf8 > 0)
		{
			waiting_utf8--;

			ASSERT((ch & 0xC0) == 0x80);

			putchar(ch);
			MUST(buffers[index].insert(ch, col++));
			if (waiting_utf8 == 0)
			{
				printf("\e[s%s\e[u", buffers[index].data() + col);
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

			putchar(ch);
			MUST(buffers[index].insert(ch, col++));
			continue;
		}

		switch (ch)
		{
		case '\e':
		{
			ch = getchar();
			if (ch != '[')
				break;
			ch = getchar();

			int value = 0;
			while (isdigit(ch))
			{
				value = (value * 10) + (ch - '0');
				ch = getchar();
			}

			switch (ch)
			{
				case 'A': if (index > 0)					{ index--; col = buffers[index].size(); printf("\e[%dG%s\e[K", prompt_length() + 1, buffers[index].data()); fflush(stdout); } break;
				case 'B': if (index < buffers.size() - 1)	{ index++; col = buffers[index].size(); printf("\e[%dG%s\e[K", prompt_length() + 1, buffers[index].data()); fflush(stdout); } break;
				case 'C': if (col < buffers[index].size())	{ col++; while ((buffers[index][col - 1] & 0xC0) == 0x80) col++; printf("\e[C"); fflush(stdout); } break;
				case 'D': if (col > 0)						{ while ((buffers[index][col - 1] & 0xC0) == 0x80) col--; col--; printf("\e[D"); fflush(stdout); } break;
				case '~':
					switch (value)
					{
						case 3: // delete
							if (col >= buffers[index].size())
								break;
							buffers[index].remove(col);
							while (col < buffers[index].size() && (buffers[index][col] & 0xC0) == 0x80)
								buffers[index].remove(col);
							printf("\e[s%s \e[u", buffers[index].data() + col);
							fflush(stdout);
							break;
					}
			}
			break;
		}
		case '\x0C': // ^L
		{
			int x = prompt_length() + character_length(buffers[index].sv().substring(col)) + 1;
			printf("\e[H\e[J");
			print_prompt();
			printf("%s\e[u\e[1;%dH", buffers[index].data(), x);
			fflush(stdout);
			break;
		}
		case '\b':
			if (col <= 0)
				break;
			while ((buffers[index][col - 1] & 0xC0) == 0x80)
				col--;
			col--;
			printf("\e[D");
			fflush(stdout);
			break;
		case '\x01': // ^A
			col = 0;
			printf("\e[%dG", prompt_length() + 1);
			fflush(stdout);
			break;
		case '\x03': // ^C
			putchar('\n');
			print_prompt();
			buffers[index].clear();
			col = 0;
			break;
		case '\x04': // ^D
			putchar('\n');
			return 0;
		case '\x7F': // backspace
			if (col <= 0)
				break;
			while ((buffers[index][col - 1] & 0xC0) == 0x80)
				buffers[index].remove(--col);
			buffers[index].remove(--col);
			printf("\b\e[s%s \e[u", buffers[index].data() + col);
			fflush(stdout);
			break;
		case '\n':
			putchar('\n');
			if (!buffers[index].empty())
			{
				parse_and_execute_command(buffers[index]);
				MUST(history.push_back(buffers[index]));
				buffers = history;
				MUST(buffers.emplace_back(""_sv));
			}
			print_prompt();
			index = buffers.size() - 1;
			col = 0;
			break;
		case '\t':
		{
			if (col != buffers[index].size())
				continue;

			if (tab_completions.has_value())
			{
				ASSERT(tab_completions->size() >= 2);

				if (!tab_index.has_value())
					tab_index = 0;
				else
				{
					MUST(buffers[index].resize(tab_completion_keep));
					col = tab_completion_keep;
					*tab_index = (*tab_index + 1) % tab_completions->size();
				}

				MUST(buffers[index].append(tab_completions.value()[*tab_index]));
				col += tab_completions.value()[*tab_index].size();

				printf("\e[%dG%s\e[K", prompt_length() + 1, buffers[index].data());
				fflush(stdout);

				break;
			}

			tab_completion_keep = col;
			auto [should_escape_spaces, prefix, completions] = list_tab_completion_entries(buffers[index].sv().substring(0, tab_completion_keep));

			if (completions.empty())
				break;

			size_t all_match_len = 0;
			for (;;)
			{
				if (completions.front().size() <= all_match_len)
					break;
				const char target = completions.front()[all_match_len];

				bool all_matched = true;
				for (const auto& completion : completions)
				{
					if (completion.size() > all_match_len && completion[all_match_len] == target)
						continue;
					all_matched = false;
					break;
				}

				if (!all_matched)
					break;
				all_match_len++;
			}

			if (all_match_len)
			{
				col += all_match_len;
				MUST(buffers[index].append(completions.front().sv().substring(0, all_match_len)));
				printf("%.*s", (int)all_match_len, completions.front().data());
				fflush(stdout);
				break;
			}

			if (completions.size() == 1)
			{
				ASSERT(all_match_len == completions.front().size());
				break;
			}

			BAN::sort::sort(completions.begin(), completions.end(),
				[](const BAN::String& a, const BAN::String& b) {
					if (auto cmp = strcmp(a.data(), b.data()))
						return cmp < 0;
					return a.size() < b.size();
				}
			);

			printf("\n");
			for (size_t i = 0; i < completions.size(); i++)
			{
				if (i != 0)
					printf(" ");
				const char* format = completions[i].sv().contains(' ') ? "'%.*s%s'" : "%.*s%s";
				printf(format, (int)prefix.size(), prefix.data(), completions[i].data());
			}
			printf("\n");
			print_prompt();
			printf("%s", buffers[index].data());
			fflush(stdout);

			if (should_escape_spaces)
			{
				for (auto& completion : completions)
				{
					for (size_t i = 0; i < completion.size(); i++)
					{
						if (!isspace(completion[i]))
							continue;
						MUST(completion.insert('\\', i));
						i++;
					}
				}
			}

			tab_completion_keep = col;
			tab_completions = BAN::move(completions);

			break;
		}
		default:
			MUST(buffers[index].insert(ch, col++));
			if (col == buffers[index].size())
				putchar(ch);
			else
				printf("%c\e[s%s\e[u", ch, buffers[index].data() + col);
			fflush(stdout);
			break;
		}
	}
}
