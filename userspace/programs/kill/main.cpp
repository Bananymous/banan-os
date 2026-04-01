#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
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

[[noreturn]] static void exit_with_error(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(1);
	__builtin_unreachable();
}

static int signal_name_to_number(const char* name)
{
	for (const auto& signal : s_signals)
		if (strcasecmp(signal.name + 3, name) == 0 || strcasecmp(signal.name, name) == 0)
			return signal.value;
	exit_with_error("unknown signal '%s'\n", name);
}

static bool parse_int_safe(const char* string, int& out)
{
	errno = 0;
	char* endptr;
	out = strtol(string, &endptr, 0);
	return *endptr == '\0' && errno == 0;
}

static int parse_int(const char* string)
{
	int result;
	if (!parse_int_safe(string, result))
		exit_with_error("invalid integer '%s'\n", string);
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
			fprintf(stderr, "unknown signal: %s\n", *signals);

		signals++;
	}

	return ret;
}

int main(int argc, char** argv)
{
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
				exit_with_error("missing signal name\n");
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
		exit_with_error("missing pids\n");

	DIR* proc_dirp = opendir("/proc");
	if (proc_dirp == NULL)
		perror("opendir");

	for (; i < argc; i++)
	{
		pid_t pid;
		if (parse_int_safe(argv[i], pid))
		{
			if (kill(pid, sig) == -1)
				perror("kill");
			continue;
		}

		const size_t arg_len = strlen(argv[i]);
		if (proc_dirp == NULL || arg_len >= NAME_MAX)
		{
			fprintf(stderr, "cannot find process \"%s\"\n", argv[i]);
			continue;
		}

		bool found = false;

		dirent* dirent;
		rewinddir(proc_dirp);
		while ((dirent = readdir(proc_dirp)))
		{
			if (dirent->d_type != DT_DIR || !isdigit(dirent->d_name[0]))
				continue;

			char path_buffer[PATH_MAX + 8];
			sprintf(path_buffer, "%s/cmdline", dirent->d_name);

			int fd = openat(dirfd(proc_dirp), path_buffer, O_RDONLY);
			if (fd == -1)
				continue;

			char buffer[NAME_MAX + 1];
			const ssize_t nread = read(fd, buffer, arg_len + 1);
			close(fd);

			if (nread != static_cast<ssize_t>(arg_len + 1) || strcmp(argv[i], buffer) != 0)
				continue;

			if (kill(parse_int(dirent->d_name), sig) == -1)
				perror("kill");
			found = true;
		}

		if (!found)
			fprintf(stderr, "cannot find process \"%s\"\n", argv[i]);
	}

	if (proc_dirp)
		closedir(proc_dirp);

	return 0;
}
