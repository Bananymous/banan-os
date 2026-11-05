#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct signal_t
{
	const char* name;
	int value;
};

static constexpr signal_t s_signals[] {
#define SIGNAL(name) { #name, name }
	SIGNAL(SIGABRT),
	SIGNAL(SIGALRM),
	SIGNAL(SIGBUS),
	SIGNAL(SIGCHLD),
	SIGNAL(SIGCONT),
	SIGNAL(SIGFPE),
	SIGNAL(SIGHUP),
	SIGNAL(SIGILL),
	SIGNAL(SIGINT),
	SIGNAL(SIGKILL),
	SIGNAL(SIGPIPE),
	SIGNAL(SIGQUIT),
	SIGNAL(SIGSEGV),
	SIGNAL(SIGSTOP),
	SIGNAL(SIGTERM),
	SIGNAL(SIGTSTP),
	SIGNAL(SIGTTIN),
	SIGNAL(SIGTTOU),
	SIGNAL(SIGUSR1),
	SIGNAL(SIGUSR2),
	SIGNAL(SIGPOLL),
	SIGNAL(SIGPROF),
	SIGNAL(SIGSYS),
	SIGNAL(SIGTRAP),
	SIGNAL(SIGURG),
	SIGNAL(SIGVTALRM),
	SIGNAL(SIGXCPU),
	SIGNAL(SIGXFSZ),
	SIGNAL(SIGWINCH),
	SIGNAL(SIGCANCEL),
#undef SIGNAL
};

static int signal_name_to_number(const char* name)
{
	for (const auto& signal : s_signals)
		if (strcasecmp(signal.name + 3, name) == 0 || strcasecmp(signal.name, name) == 0)
			return signal.value;
	errx(1, "unknown signal `%s'", name);
}

static bool parse_int_safe(const char* string, int& out)
{
	char* endptr;
	out = ({ errno = 0; strtol(string, &endptr, 0); });
	return *endptr == '\0' && errno == 0;
}

static int parse_int(const char* string)
{
	int result;
	if (!parse_int_safe(string, result))
		errx(1, "invalid integer `%s'", string);
	return result;
}

static int list_signals(char** signals)
{
	if (signals[0] == nullptr)
	{
		for (const auto& signal : s_signals)
		{
			if (&signal != s_signals)
				printf(" ");
			printf("%s", signal.name);
		}
		printf("\n");
		return 0;
	}

	int ret = 0;

	while (*signals)
	{
		bool found = false;

		if (isdigit(**signals))
		{
			const int sig = parse_int(*signals);
			for (const auto& signal : s_signals)
			{
				if (signal.value != sig)
					continue;
				printf("%s\n", signal.name + 3);
				found = true;
				break;
			}
		}
		else
		{
			for (const auto& signal : s_signals)
			{
				if (strcasecmp(signal.name, *signals) != 0 && strcasecmp(signal.name + 3, *signals) != 0)
					continue;
				printf("%d\n", signal.value);
				found = true;
				break;
			}
		}

		if (!found)
		{
			warnx("unknown signal `%s'", *signals);
			ret = 1;
		}

		signals++;
	}

	return ret;
}

int main(int argc, char** argv)
{
	setprogname(argv[0]);

	if (argc >= 2 && strcmp(argv[1], "-l") == 0)
		return list_signals(argv + 2);

	int sig = SIGTERM;

	int i = 1;
	for (; i < argc; i++)
	{
		if (argv[i][0] != '-')
			break;
		if (argv[i][1] == '-' && argv[i][2] == '\0')
		{
			i++;
			break;
		}

		if (argv[i][1] == 's' && argv[i][2] == '\0')
		{
			if (i + 1 >= argc)
				errx(1, "missing signal name");
			sig = signal_name_to_number(argv[i + 1]);
			i++;
		}
		else if (argv[i][2] == '-' || isdigit(argv[i][1]))
		{
			sig = parse_int(argv[i] + 1);
		}
		else
		{
			sig = signal_name_to_number(argv[i] + 1);
		}
	}

	if (i >= argc)
		errx(1, "missing PIDs");

	int ret = 0;

	for (; i < argc; i++)
	{
		pid_t pid;
		if (!parse_int_safe(argv[i], pid))
		{
			warnx("invalid PID `%s'", argv[i]);
			ret = 1;
		}
		else if (kill(pid, sig) == -1)
		{
			warn("kill `%s'", argv[i]);
			ret = 1;
		}
	}

	return ret;
}
