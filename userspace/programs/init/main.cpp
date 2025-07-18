#include <BAN/String.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/banan-os.h>
#include <termios.h>

int main(int argc, char** argv)
{
	ASSERT(argc == 2);

	const char* tty_name = argv[1];

	if (open(tty_name, O_RDONLY | O_TTY_INIT) != 0) _exit(1);
	if (open(tty_name, O_WRONLY) != 1) _exit(1);
	if (open(tty_name, O_WRONLY) != 2) _exit(1);
	if (open("/dev/debug", O_WRONLY) != 3) _exit(1);

	if (signal(SIGINT, [](int) {}) == SIG_ERR)
		perror("signal");

	if (load_keymap("/usr/share/keymaps/us.keymap") == -1)
		perror("load_keymap");

	setenv("TERM", "ansi", 1);

	if (fork() == 0)
	{
		execl("/bin/dhcp-client", "dhcp-client", NULL);
		exit(1);
	}

	if (fork() == 0)
	{
		execl("/bin/resolver", "resolver", NULL);
		exit(1);
	}

	bool first = true;

	termios termios;
	tcgetattr(STDIN_FILENO, &termios);

	while (true)
	{
		tcsetattr(STDIN_FILENO, TCSANOW, &termios);

		char name_buffer[128];

		while (!first)
		{
			printf("username: ");
			fflush(stdout);

			ssize_t nread = read(STDIN_FILENO, name_buffer, sizeof(name_buffer) - 1);
			if (nread == -1)
			{
				perror("read");
				return 1;
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

		if (chown(tty_name, pwd->pw_uid, 0) == -1)
			perror("chown");

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

			setenv("PATH", "/bin:/usr/bin", 0);

			setenv("HOME", pwd->pw_dir, 1);
			chdir(pwd->pw_dir);

			setenv("SHELL", pwd->pw_shell, 1);
			char shell_path[PATH_MAX];
			strcpy(shell_path, pwd->pw_shell);

			endpwent();

			execl(shell_path, shell_path, nullptr);
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

		if (chown(tty_name, 0, 0) == -1)
			perror("chown");
	}

}
