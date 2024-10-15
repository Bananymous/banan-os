#include "CommandTypes.h"
#include "Execute.h"

#include <BAN/ScopeGuard.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

extern int g_pid;
extern int g_argc;
extern char** g_argv;

BAN::ErrorOr<BAN::String> CommandArgument::evaluate(Execute& execute) const
{
	static_assert(
		BAN::is_same_v<CommandArgument::ArgumentPart,
			BAN::Variant<
				FixedString,
				EnvironmentVariable,
				BuiltinVariable,
				CommandTree
			>
		>
	);

	BAN::String evaluated;

	for (const auto& part : parts)
	{
		ASSERT(part.has_value());
		if (part.has<FixedString>())
			TRY(evaluated.append(part.get<FixedString>().value));
		else if (part.has<EnvironmentVariable>())
		{
			const char* env = getenv(part.get<EnvironmentVariable>().value.data());
			if (env != nullptr)
				TRY(evaluated.append(env));
		}
		else if (part.has<BuiltinVariable>())
		{
			const auto& builtin = part.get<BuiltinVariable>();
			ASSERT(!builtin.value.empty());

			if (!isdigit(builtin.value.front()))
			{
				ASSERT(builtin.value.size() == 1);
				switch (builtin.value.front())
				{
					case '_':
					case '@':
					case '*':
					case '-':
						fprintf(stderr, "TODO: $%c\n", builtin.value.front());
						break;
					case '$':
						evaluated = TRY(BAN::String::formatted("{}", g_pid));
						break;
					case '#':
						evaluated = TRY(BAN::String::formatted("{}", g_argc - 1));
						break;
					case '?':
						evaluated = TRY(BAN::String::formatted("{}", execute.last_return_value()));
						break;
					case '!':
						evaluated = TRY(BAN::String::formatted("{}", execute.last_background_pid()));
						break;
					default:
						ASSERT_NOT_REACHED();
				}
			}
			else
			{
				int argv_index = 0;
				for (char c : builtin.value)
				{
					ASSERT(isdigit(c));
					if (BAN::Math::will_multiplication_overflow<int>(argv_index, 10) ||
						BAN::Math::will_addition_overflow<int>(argv_index * 10, c - '0'))
					{
						argv_index = INT_MAX;
						fprintf(stderr, "integer overflow, capping at %d\n", argv_index);
						break;
					}
					argv_index = (argv_index * 10) + (c - '0');
				}

				if (argv_index < g_argc)
					TRY(evaluated.append(const_cast<const char*>(g_argv[argv_index])));
			}
		}
		else if (part.has<CommandTree>())
		{
			// FIXME: this should resolve to multiple arguments if not double quoted

			int execute_pipe[2];
			if (pipe(execute_pipe) == -1)
				return BAN::Error::from_errno(errno);
			BAN::ScopeGuard pipe_rd_closer([execute_pipe] { close(execute_pipe[0]); });
			BAN::ScopeGuard pipe_wr_closer([execute_pipe] { close(execute_pipe[1]); });

			const pid_t child_pid = fork();
			if (child_pid == -1)
				return BAN::Error::from_errno(errno);
			if (child_pid == 0)
			{
				if (dup2(execute_pipe[1], STDOUT_FILENO) == -1)
					return BAN::Error::from_errno(errno);
				setpgrp();
				auto ret = execute.execute_command(part.get<CommandTree>());
				if (ret.is_error())
					exit(ret.error().get_error_code());
				exit(execute.last_return_value());
			}

			pipe_wr_closer.disable();
			close(execute_pipe[1]);

			char buffer[128];
			while (true)
			{
				const ssize_t nread = read(execute_pipe[0], buffer, sizeof(buffer));
				if (nread < 0)
					perror("read");
				if (nread <= 0)
					break;
				TRY(evaluated.append(BAN::StringView(buffer, nread)));
			}

			while (!evaluated.empty() && isspace(evaluated.back()))
				evaluated.pop_back();
		}
		else
		{
			ASSERT_NOT_REACHED();
		}
	}

	return evaluated;
}
