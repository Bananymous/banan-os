#include "Alias.h"
#include "Builtin.h"
#include "Execute.h"

#include <ctype.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ERROR_RETURN(__msg, __ret) do { perror(__msg); return __ret; } while (false)

void Builtin::initialize()
{
	MUST(m_builtin_commands.emplace("clear"_sv,
		[](Execute&, BAN::Span<const BAN::String>, FILE*, FILE* fout) -> int
		{
			fprintf(fout, "\e[H\e[3J\e[2J");
			fflush(fout);
			return 0;
		}, true
	));

	MUST(m_builtin_commands.emplace("exit"_sv,
		[](Execute&, BAN::Span<const BAN::String> arguments, FILE*, FILE*) -> int
		{
			int exit_code = 0;
			if (arguments.size() > 1)
			{
				auto exit_string = arguments[1].sv();
				for (size_t i = 0; i < exit_string.size() && isdigit(exit_string[i]); i++)
					exit_code = (exit_code * 10) + (exit_string[i] - '0');
			}
			exit(exit_code);
			ASSERT_NOT_REACHED();
		}, true
	));

	MUST(m_builtin_commands.emplace("export"_sv,
		[](Execute&, BAN::Span<const BAN::String> arguments, FILE*, FILE*) -> int
		{
			bool first = false;
			for (const auto& argument : arguments)
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
		}, true
	));

	MUST(m_builtin_commands.emplace("unset"_sv,
		[](Execute&, BAN::Span<const BAN::String> arguments, FILE*, FILE*) -> int
		{
			for (const auto& argument : arguments)
				if (unsetenv(argument.data()) == -1)
					ERROR_RETURN("unsetenv", 1);
			return 0;
		}, true
	));

	MUST(m_builtin_commands.emplace("alias"_sv,
		[](Execute&, BAN::Span<const BAN::String> arguments, FILE*, FILE* fout) -> int
		{
			if (arguments.size() == 1)
			{
				Alias::get().for_each_alias(
					[fout](BAN::StringView name, BAN::StringView value) -> BAN::Iteration
					{
						fprintf(fout, "%.*s='%.*s'\n",
							(int)name.size(), name.data(),
							(int)value.size(), value.data()
						);
						return BAN::Iteration::Continue;
					}
				);
				return 0;
			}

			for (size_t i = 1; i < arguments.size(); i++)
			{
				auto idx = arguments[i].sv().find('=');
				if (idx.has_value() && idx.value() == 0)
					continue;
				if (!idx.has_value())
				{
					auto value = Alias::get().get_alias(arguments[i]);
					if (value.has_value())
						fprintf(fout, "%s='%.*s'\n", arguments[i].data(), (int)value->size(), value->data());
				}
				else
				{
					auto alias = arguments[i].sv().substring(0, idx.value());
					auto value = arguments[i].sv().substring(idx.value() + 1);
					if (auto ret = Alias::get().set_alias(alias, value); ret.is_error())
						fprintf(stderr, "could not set alias: %s\n", ret.error().get_message());
				}
			}

			return 0;
		}, true
	));

	MUST(m_builtin_commands.emplace("source"_sv,
		[](Execute& execute, BAN::Span<const BAN::String> arguments, FILE*, FILE* fout) -> int
		{
			if (arguments.size() != 2)
			{
				fprintf(fout, "usage: source FILE\n");
				return 1;
			}
			if (execute.source_script(arguments[1]).is_error())
				return 1;
			return 0;
		}, true
	));

	MUST(m_builtin_commands.emplace("cd"_sv,
		[](Execute&, BAN::Span<const BAN::String> arguments, FILE*, FILE* fout) -> int
		{
			if (arguments.size() > 2)
			{
				fprintf(fout, "cd: too many arguments\n");
				return 1;
			}

			const char* path = nullptr;
			if (arguments.size() == 2)
				path = arguments[1].data();
			else
				path = getenv("HOME");

			if (path == nullptr)
				return 0;

			if (chdir(path) == -1)
				ERROR_RETURN("chdir", 1);

			setenv("PWD", path, 1);

			return 0;
		}, true
	));

	MUST(m_builtin_commands.emplace("type"_sv,
		[](Execute&, BAN::Span<const BAN::String> arguments, FILE*, FILE* fout) -> int
		{
			const auto is_executable_file =
				[](const char* path) -> bool
				{
					struct stat st;
					if (stat(path, &st) == -1)
						return false;
					if (!(st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
						return false;
					return true;
				};

			if (!arguments.empty())
				arguments = arguments.slice(1);

			BAN::Vector<BAN::StringView> path_dirs;
			if (const char* path_env = getenv("PATH"))
				if (auto split_ret = BAN::StringView(path_env ? path_env : "").split(':'); !split_ret.is_error())
					path_dirs = split_ret.release_value();

			for (const auto& argument : arguments)
			{
				if (auto alias = Alias::get().get_alias(argument); alias.has_value())
				{
					fprintf(fout, "%s is an alias for %s\n", argument.data(), alias->data());
					continue;
				}

				if (Builtin::get().find_builtin(argument))
				{
					fprintf(fout, "%s is a shell builtin\n", argument.data());
					continue;
				}

				if (argument.sv().contains('/'))
				{
					if (is_executable_file(argument.data()))
					{
						fprintf(fout, "%s is %s\n", argument.data(), argument.data());
						continue;
					}
				}
				else
				{
					bool found = false;
					for (const auto& path_dir : path_dirs)
					{
						char path_buffer[PATH_MAX];
						memcpy(path_buffer,                   path_dir.data(), path_dir.size());
						memcpy(path_buffer + path_dir.size(), argument.data(), argument.size());
						path_buffer[path_dir.size() + argument.size()] = '\0';

						if (is_executable_file(path_buffer))
						{
							fprintf(fout, "%s is %s\n", argument.data(), path_buffer);
							found = true;
							break;
						}
					}
					if (found)
						continue;

				}

				fprintf(fout, "%s not found\n", argument.data());
			}

			return 0;
		}, true
	));

	// FIXME: time should not actually be a builtin command but a shell reserved keyword
	//        e.g. `time foobar=lol sh -c 'echo $foobar'` should resolve set foobar env
	MUST(m_builtin_commands.emplace("time"_sv,
		[](Execute& execute, BAN::Span<const BAN::String> arguments, FILE* fin, FILE* fout) -> int
		{
			timespec start, end;

			if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
				ERROR_RETURN("clock_gettime", 1);

			auto execute_ret = execute.execute_command_sync(arguments.slice(1), fileno(fin), fileno(fout));

			if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
				ERROR_RETURN("clock_gettime", 1);

			uint64_t total_ns = 0;
			total_ns += (end.tv_sec - start.tv_sec) * 1'000'000'000;
			total_ns += end.tv_nsec - start.tv_nsec;

			int secs  =  total_ns / 1'000'000'000;
			int msecs = (total_ns % 1'000'000'000) / 1'000'000;

			fprintf(fout, "took %d.%03d s\n", secs, msecs);

			if (execute_ret.is_error())
				return 256 + execute_ret.error().get_error_code();
			return execute_ret.value();
		}, false
	));
}

void Builtin::for_each_builtin(BAN::Function<BAN::Iteration(BAN::StringView, const BuiltinCommand&)> callback) const
{
	for (const auto& [name, function] : m_builtin_commands)
	{
		switch (callback(name.sv(), function))
		{
			case BAN::Iteration::Break:
				break;
			case BAN::Iteration::Continue:
				continue;;
		}
		break;
	}
}

const Builtin::BuiltinCommand* Builtin::find_builtin(const BAN::String& name) const
{
	auto it = m_builtin_commands.find(name);
	if (it == m_builtin_commands.end())
		return nullptr;
	return &it->value;
}

BAN::ErrorOr<int> Builtin::BuiltinCommand::execute(Execute& execute, BAN::Span<const BAN::String> arguments, int fd_in, int fd_out) const
{
	const auto fd_to_file =
		[](int fd, FILE* file, const char* mode) -> BAN::ErrorOr<FILE*>
		{
			if (fd == fileno(file))
				return file;
			int fd_dup = dup(fd);
			if (fd_dup == -1)
				return BAN::Error::from_errno(errno);
			file = fdopen(fd_dup, mode);
			if (file == nullptr)
				return BAN::Error::from_errno(errno);
			return file;
		};

	FILE* fin  = TRY(fd_to_file(fd_in,  stdin,  "r"));
	FILE* fout = TRY(fd_to_file(fd_out, stdout, "w"));
	int ret = function(execute, arguments, fin, fout);
	if (fileno(fin)  != fd_in ) fclose(fin);
	if (fileno(fout) != fd_out) fclose(fout);

	return ret;
}
