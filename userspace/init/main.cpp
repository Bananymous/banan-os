#include <BAN/String.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct User
{
	BAN::String name;
	uid_t uid;
	gid_t gid;
	BAN::String home;
	BAN::String shell;
};

BAN::Optional<id_t> parse_id(BAN::StringView value)
{
	// NOTE: we only allow 10^9 uids
	if (value.size() < 1 || value.size() > 9)
		return {};

	id_t id { 0 };
	for (char c : value)
	{
		if (!isdigit(c))
			return {};
		id = (id * 10) + (c - '0');
	}

	return id;
}

BAN::Optional<User> parse_user(BAN::StringView line)
{
	auto parts = MUST(line.split(':', true));
	if (parts.size() != 7)
		return {};
	User user;
	user.name = parts[0];
	user.uid = ({ auto id = parse_id(parts[2]); if (!id.has_value()) return {}; id.value(); });
	user.gid = ({ auto id = parse_id(parts[3]); if (!id.has_value()) return {}; id.value(); });
	user.home = parts[5];
	user.shell = parts[6];
	return user;
}

BAN::Vector<User> parse_users()
{
	FILE* fp = fopen("/etc/passwd", "r");
	if (fp == nullptr)
	{
		fprintf(stderr, "could not open /etc/passwd\n");
		perror("fopen");
	}

	BAN::Vector<User> users;

	char buffer[1024];
	while (fgets(buffer, sizeof(buffer), fp))
	{
		*strchrnul(buffer, '\n') = '\0';
		auto user = parse_user(buffer);
		if (user.has_value())
			MUST(users.push_back(user.release_value()));
	}

	if (ferror(fp))
	{
		perror("fread");
		fclose(fp);
		return {};
	}

	fclose(fp);

	return users;
}

void initialize_stdio()
{
	char tty[L_ctermid];
	ctermid(tty);
	if (open(tty, O_RDONLY) != 0) _exit(1);
	if (open(tty, O_WRONLY) != 1) _exit(1);
	if (open(tty, O_WRONLY) != 2) _exit(1);
}

int main()
{
	initialize_stdio();

	bool first = true;

	while (true)
	{
		auto users = parse_users();

		char name_buffer[128];
		BAN::StringView name;

		if (first)
		{
			name = "user"sv;
			first = false;
		}

		while (name.empty())
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
			if (nread == 1)
				continue;		

			name = BAN::StringView(name_buffer, nread - 1);
			break;
		}

		for (const User& user : users)
		{
			if (user.name == name)
			{
				pid_t pid = fork();
				if (pid == 0)
				{
					printf("Welcome back %s!\n", user.name.data());

					if (setgid(user.gid) == -1)
						perror("setgid");
					if (setuid(user.uid) == -1)
						perror("setuid");

					execl(user.shell.data(), user.shell.data(), nullptr);
					perror("execl");
					
					exit(1);
				}
				
				if (pid == -1)
				{
					perror("fork");
					break;
				}

				int status;
				waitpid(pid, &status, 0);
			}
		}
	}


}
