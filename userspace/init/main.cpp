#include <BAN/String.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void initialize_stdio()
{
	const char* tty = "/dev/tty0";
	if (open(tty, O_RDONLY | O_TTY_INIT) != 0) _exit(1);
	if (open(tty, O_WRONLY) != 1) _exit(1);
	if (open(tty, O_WRONLY) != 2) _exit(1);
}

int main()
{
	initialize_stdio();

	if (signal(SIGINT, [](int) {}) == SIG_ERR)
		perror("signal");

	bool first = true;

	while (true)
	{
		char name_buffer[128];

		while (!first)
		{
			printf("username: ");
			fflush(stdout);

			size_t nread = fread(name_buffer, 1, sizeof(name_buffer) - 1, stdin);
			if (nread == 0)
			{
				if (ferror(stdin))
				{
					fprintf(stderr, "Could not read from stdin\n");
					return 1;
				}
				continue;
			}
			if (nread <= 1 || name_buffer[nread - 1] != '\n')
				continue;
			name_buffer[nread - 1] = '\0';
			break;
		}

		if (first)
		{
			strcpy(name_buffer, "user");
			first = false;
		}

		auto* pwd = getpwnam(name_buffer);
		if (pwd == nullptr)
			continue;

		pid_t pid = fork();
		if (pid == 0)
		{
			pid_t pgrp = setpgrp();
			if (tcsetpgrp(0, pgrp) == -1)
			{
				perror("tcsetpgrp");
				exit(1);
			}

			printf("Welcome back %s!\n", pwd->pw_name);

			if (setgid(pwd->pw_gid) == -1)
				perror("setgid");
			if (setuid(pwd->pw_uid) == -1)
				perror("setuid");

			setenv("HOME", pwd->pw_dir, 1);
			chdir(pwd->pw_dir);

			execl(pwd->pw_shell, pwd->pw_shell, nullptr);
			perror("execl");

			exit(1);
		}
		
		endpwent();
		
		if (pid == -1)
		{
			perror("fork");
			break;
		}

		int status;
		waitpid(pid, &status, 0);

		if (tcsetpgrp(0, getpgrp()) == -1)
			perror("tcsetpgrp");
	}

}
