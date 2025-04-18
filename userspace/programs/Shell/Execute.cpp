#include "Builtin.h"
#include "Execute.h"
#include "TokenParser.h"

#include <BAN/ScopeGuard.h>

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define CHECK_FD_OR_PERROR_AND_EXIT(oldfd, newfd) ({ if ((oldfd) != (newfd) && dup2((oldfd), (newfd)) == -1) { perror("dup2"); exit(errno); } })
#define TRY_OR_PERROR_AND_BREAK(expr) ({ auto&& eval = (expr); if (eval.is_error()) { fprintf(stderr, "%s\n", eval.error().get_message()); continue; } eval.release_value(); })
#define TRY_OR_EXIT(expr) ({ auto&& eval = (expr); if (eval.is_error()) exit(eval.error().get_error_code()); eval.release_value(); })

static BAN::ErrorOr<BAN::String> find_absolute_path_of_executable(const BAN::String& command)
{
	if (command.size() >= PATH_MAX)
		return BAN::Error::from_errno(ENAMETOOLONG);

	const auto check_executable_file =
		[](const char* path) -> BAN::ErrorOr<void>
		{
			struct stat st;
			if (stat(path, &st) == -1)
				return BAN::Error::from_errno(errno);
			if (!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
				return BAN::Error::from_errno(ENOEXEC);
			return {};
		};

	if (command.sv().contains('/'))
	{
		TRY(check_executable_file(command.data()));
		return TRY(BAN::String::formatted("{}", command));
	}

	const char* path_env = getenv("PATH");
	if (path_env == nullptr)
		return BAN::Error::from_errno(ENOENT);

	auto path_dirs = TRY(BAN::StringView(path_env).split(':'));
	for (auto path_dir : path_dirs)
	{
		const auto absolute_path = TRY(BAN::String::formatted("{}/{}", path_dir, command));

		auto check_result = check_executable_file(absolute_path.data());
		if (!check_result.is_error())
			return absolute_path;

		if (check_result.error().get_error_code() == ENOENT)
			continue;
		return check_result.release_error();
	}

	return BAN::Error::from_errno(ENOENT);
}

BAN::ErrorOr<Execute::ExecuteResult> Execute::execute_command_no_wait(const InternalCommand& command)
{
	ASSERT(!command.arguments.empty());

	if (command.command.has<Builtin::BuiltinCommand>() && !command.background)
	{
		const auto& builtin = command.command.get<Builtin::BuiltinCommand>();
		if (builtin.immediate)
		{
			return ExecuteResult {
				.pid = -1,
				.exit_code = TRY(builtin.execute(*this, command.arguments, command.fd_in, command.fd_out))
			};
		}
	}

	const pid_t child_pid = fork();
	if (child_pid == -1)
		return BAN::Error::from_errno(errno);
	if (child_pid == 0)
	{
		if (command.command.has<Builtin::BuiltinCommand>())
		{
			auto builtin_ret = command.command.get<Builtin::BuiltinCommand>().execute(*this, command.arguments, command.fd_in, command.fd_out);
			if (builtin_ret.is_error())
				exit(builtin_ret.error().get_error_code());
			exit(builtin_ret.value());
		}

		for (const auto& environment : command.environments)
			setenv(environment.name.data(), environment.value.data(), true);

		BAN::Vector<const char*> exec_args;
		TRY_OR_EXIT(exec_args.reserve(command.arguments.size() + 1));
		for (const auto& argument : command.arguments)
			TRY_OR_EXIT(exec_args.push_back(argument.data()));
		TRY_OR_EXIT(exec_args.push_back(nullptr));

		CHECK_FD_OR_PERROR_AND_EXIT(command.fd_in, STDIN_FILENO);
		CHECK_FD_OR_PERROR_AND_EXIT(command.fd_out, STDOUT_FILENO);

		execv(command.command.get<BAN::String>().data(), const_cast<char* const*>(exec_args.data()));
		exit(errno);
	}

	if (setpgid(child_pid, command.pgrp ? command.pgrp : child_pid))
		perror("setpgid");
	if (!command.background && command.pgrp == 0 && isatty(STDIN_FILENO))
		if (tcsetpgrp(STDIN_FILENO, child_pid) == -1)
			perror("tcsetpgrp");

	return ExecuteResult {
		.pid = child_pid,
		.exit_code = -1,
	};
}

BAN::ErrorOr<int> Execute::execute_command_sync(BAN::Span<const BAN::String> arguments, int fd_in, int fd_out)
{
	if (arguments.empty())
		return 0;

	InternalCommand command {
		.command = {},
		.arguments = arguments,
		.environments = {},
		.fd_in = fd_in,
		.fd_out = fd_out,
		.background = false,
		.pgrp = getpgrp(),
	};

	if (const auto* builtin = Builtin::get().find_builtin(arguments[0]))
		command.command = *builtin;
	else
	{
		auto absolute_path_or_error = find_absolute_path_of_executable(arguments[0]);
		if (absolute_path_or_error.is_error())
		{
			if (absolute_path_or_error.error().get_error_code() == ENOENT)
			{
				fprintf(stderr, "command not found: %s\n", arguments[0].data());
				return 127;
			}
			fprintf(stderr, "could not execute command: %s\n", absolute_path_or_error.error().get_message());
			return 126;
		}
		command.command = absolute_path_or_error.release_value();
	}

	const auto execute_result = TRY(execute_command_no_wait(command));
	if (execute_result.pid == -1)
		return execute_result.exit_code;

	int status;
	if (waitpid(execute_result.pid, &status, 0) == -1)
		return BAN::Error::from_errno(errno);

	if (!WIFSIGNALED(status))
		return WEXITSTATUS(status);
	return 128 + WTERMSIG(status);
}

BAN::ErrorOr<void> Execute::execute_command(const PipedCommand& piped_command)
{
	ASSERT(!piped_command.commands.empty());

	int last_pipe_rd = STDIN_FILENO;

	BAN::Vector<pid_t> child_pids;
	TRY(child_pids.resize(piped_command.commands.size(), 0));

	BAN::Vector<int> child_codes;
	TRY(child_codes.resize(piped_command.commands.size(), 126));

	const auto evaluate_arguments =
		[this](BAN::Span<const CommandArgument> arguments) -> BAN::ErrorOr<BAN::Vector<BAN::String>>
		{
			BAN::Vector<BAN::String> result;
			TRY(result.reserve(arguments.size()));
			for (const auto& argument : arguments)
				TRY(result.push_back(TRY(argument.evaluate(*this))));
			return result;
		};

	const auto evaluate_environment =
		[this](BAN::Span<const SingleCommand::EnvironmentVariable> environments) -> BAN::ErrorOr<BAN::Vector<InternalCommand::Environment>>
		{
			BAN::Vector<InternalCommand::Environment> result;
			TRY(result.reserve(environments.size()));
			for (const auto& environment : environments)
				TRY(result.emplace_back(environment.name, TRY(environment.value.evaluate(*this))));
			return result;
		};

	const int stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
	if (stdin_flags == -1)
		perror("fcntl");

	for (size_t i = 0; i < piped_command.commands.size(); i++)
	{
		int new_pipe[2] { STDIN_FILENO, STDOUT_FILENO };
		if (i != piped_command.commands.size() - 1)
			if (pipe(new_pipe) == -1)
				return BAN::Error::from_errno(errno);

		BAN::ScopeGuard pipe_closer(
			[&]()
			{
				if (new_pipe[1] != STDOUT_FILENO)
					close(new_pipe[1]);
				if (last_pipe_rd != STDIN_FILENO)
					close(last_pipe_rd);
				last_pipe_rd = new_pipe[0];
			}
		);

		const int fd_in  = last_pipe_rd;
		const int fd_out = new_pipe[1];

		const auto arguments = TRY_OR_PERROR_AND_BREAK(evaluate_arguments(piped_command.commands[i].arguments.span()));
		const auto environments = TRY_OR_PERROR_AND_BREAK(evaluate_environment(piped_command.commands[i].environment.span()));

		InternalCommand command {
			.command = {},
			.arguments = arguments.span(),
			.environments = environments.span(),
			.fd_in = fd_in,
			.fd_out = fd_out,
			.background = piped_command.background,
			.pgrp = child_pids.front(),
		};

		if (const auto* builtin = Builtin::get().find_builtin(arguments[0]))
			command.command = *builtin;
		else
		{
			auto absolute_path_or_error = find_absolute_path_of_executable(arguments[0]);
			if (absolute_path_or_error.is_error())
			{
				if (absolute_path_or_error.error().get_error_code() == ENOENT)
				{
					fprintf(stderr, "command not found: %s\n", arguments[0].data());
					child_codes[i] = 127;
				}
				else
				{
					fprintf(stderr, "could not execute command: %s\n", absolute_path_or_error.error().get_message());
					child_codes[i] = 126;
				}
				continue;
			}
			command.command = absolute_path_or_error.release_value();
		}

		auto execute_result = TRY_OR_PERROR_AND_BREAK(execute_command_no_wait(command));
		if (execute_result.pid == -1)
			child_codes[i] = execute_result.exit_code;
		else
			child_pids[i] = execute_result.pid;
	}

	if (last_pipe_rd != STDIN_FILENO)
		close(last_pipe_rd);

	if (piped_command.background)
		return {};

	for (size_t i = 0; i < piped_command.commands.size(); i++)
	{
		if (child_pids[i] == 0)
			continue;

		int status = 0;
		if (waitpid(child_pids[i], &status, 0) == -1)
			perror("waitpid");

		if (WIFEXITED(status))
			child_codes[i] = WEXITSTATUS(status);
		else if (WIFSIGNALED(status))
			child_codes[i] = 128 + WTERMSIG(status);
		else
			ASSERT_NOT_REACHED();
	}

	if (isatty(STDIN_FILENO))
	{
		if (tcsetpgrp(0, getpgrp()) == -1)
			perror("tcsetpgrp");
		if (stdin_flags != -1 && fcntl(STDIN_FILENO, F_SETFL, stdin_flags) == -1)
			perror("fcntl");
	}
	m_last_return_value = child_codes.back();

	return {};
}

BAN::ErrorOr<void> Execute::execute_command(const CommandTree& command_tree)
{
	for (const auto& [command, condition] : command_tree.commands)
	{
		bool should_run = false;
		switch (condition)
		{
			case ConditionalCommand::Condition::Always:
				should_run = true;
				break;
			case ConditionalCommand::Condition::OnFailure:
				should_run = (m_last_return_value != 0);
				break;
			case ConditionalCommand::Condition::OnSuccess:
				should_run = (m_last_return_value == 0);
				break;
		}

		if (!should_run)
			continue;

		TRY(execute_command(command));
	}

	return {};
}

BAN::ErrorOr<void> Execute::source_script(BAN::StringView path)
{
	BAN::Vector<BAN::String> script_lines;

	{
		FILE* fp = fopen(path.data(), "r");
		if (fp == nullptr)
			return BAN::Error::from_errno(errno);

		BAN::String current;
		char temp_buffer[128];
		while (fgets(temp_buffer, sizeof(temp_buffer), fp))
		{
			TRY(current.append(temp_buffer));
			if (current.back() != '\n')
				continue;
			current.pop_back();

			if (!current.empty())
				TRY(script_lines.push_back(BAN::move(current)));
			current.clear();
		}

		if (!current.empty())
			TRY(script_lines.push_back(BAN::move(current)));

		fclose(fp);
	}

	size_t index = 0;
	TokenParser parser(
		[&](BAN::Optional<BAN::StringView>) -> BAN::Optional<BAN::String>
		{
			if (index >= script_lines.size())
				return {};
			return script_lines[index++];
		}
	);
	if (!parser.main_loop(true))
		return BAN::Error::from_literal("oop");
	return {};
}
