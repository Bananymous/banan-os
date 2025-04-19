#include "Builtin.h"
#include "Execute.h"
#include "Input.h"
#include "TokenParser.h"

#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

int g_pid;
int g_argc;
char** g_argv;

int main(int argc, char** argv)
{
	g_pid = getpid();
	g_argc = argc;
	g_argv = argv;

	{
		struct sigaction sa;
		sa.sa_flags = 0;

		sa.sa_handler = [](int) {};
		sigaction(SIGINT, &sa, nullptr);

		sa.sa_handler = SIG_IGN;
		sigaction(SIGTTOU, &sa, nullptr);
	}

	{
		char cwd_buffer[PATH_MAX];
		if (getcwd(cwd_buffer, sizeof(cwd_buffer)))
			setenv("PWD", cwd_buffer, 1);
	}

	Builtin::get().initialize();

	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-')
		{
			g_argc = g_argc - i;
			g_argv = g_argv + i;

			Execute execute;
			(void)execute.source_script(argv[i]);
			return execute.last_return_value();
		}

		if (strcmp(argv[i], "-c") == 0)
		{
			if (i + 1 >= argc)
			{
				printf("-c requires an argument\n");
				return 1;
			}

			g_argc = g_argc - (i + 2);
			g_argv = g_argv + (i + 2);

			bool got_input = false;

			TokenParser parser(
				[&](BAN::Optional<BAN::StringView>) -> BAN::Optional<BAN::String>
				{
					if (got_input)
						return {};
					got_input = true;

					BAN::String input;
					MUST(input.append(argv[i + 1]));
					return input;
				}
			);
			if (!parser.main_loop(true))
				return 126;
			return parser.execute().last_return_value();
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

	Input input;
	TokenParser parser(
		[&](BAN::Optional<BAN::StringView> prompt) -> BAN::Optional<BAN::String>
		{
			return input.get_input(prompt);
		}
	);

	if (const char* home_env = getenv("HOME"))
	{
		BAN::String config_file_path;
		MUST(config_file_path.append(home_env));
		MUST(config_file_path.append("/.shellrc"_sv));

		struct stat st;
		if (stat(config_file_path.data(), &st) == 0)
		{
			if (auto ret = parser.execute().source_script(config_file_path.sv()); ret.is_error())
				fprintf(stderr, "could not source config file at '%s': %s\n", config_file_path.data(), ret.error().get_message());
		}
	}

	if (!parser.main_loop(false))
		return 126;
	return 0;
}
