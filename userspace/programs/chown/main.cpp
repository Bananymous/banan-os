#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void usage(const char* argv0, int ret)
{
	FILE* out = (ret == 0) ? stdout : stderr;
	fprintf(out, "usage: %s [OWNER][:[GROUP]] FILE...\n", argv0);
	fprintf(out, "  Change the owner and/or group of each FILE.\n");
	exit(ret);
}

[[noreturn]] void print_error_and_exit(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(1);
	__builtin_unreachable();
}

const passwd* get_user(const char* string)
{
	bool is_numeric = true;
	for (size_t i = 0; string[i] && is_numeric; i++)
		if (!isdigit(string[i]))
			is_numeric = false;
	if (is_numeric)
		return getpwuid(atoll(string));
	return getpwnam(string);
}

const group* get_group(const char* string)
{
	bool is_numeric = true;
	for (size_t i = 0; string[i] && is_numeric; i++)
		if (!isdigit(string[i]))
			is_numeric = false;
	if (is_numeric)
		return getgrgid(atoll(string));
	return getgrnam(string);
}

int main(int argc, char** argv)
{
	if (argc <= 2)
		usage(argv[0], 1);

	uid_t uid = -1;
	gid_t gid = -1;

	const char* owner_string = argv[1];

	const char* colon = strchr(owner_string, ':');
	if (colon == owner_string)
	{
		const auto* group = get_group(owner_string + 1);
		if (group == nullptr)
			print_error_and_exit("could not find group %s\n", owner_string + 1);
		gid = group->gr_gid;
	}
	else if (colon == nullptr)
	{
		const auto* user = get_user(owner_string);
		if (user == nullptr)
			print_error_and_exit("could not find user %s\n", owner_string);
		uid = user->pw_uid;
	}
	else
	{
		char* user_name = strndup(owner_string, colon - owner_string);
		if (user_name == nullptr)
			print_error_and_exit("strndup: %s\n", strerror(errno));
		const auto* user = get_user(user_name);
		if (user == nullptr)
			print_error_and_exit("could not find user %s\n", user_name);
		free(user_name);
		uid = user->pw_uid;
		if (colon[1] == '\0')
			gid = user->pw_gid;
		else
		{
			const auto* group = get_group(colon + 1);
			if (group == nullptr)
				print_error_and_exit("could not find group %s\n", colon + 1);
			gid = group->gr_gid;
		}
	}

	int ret = 0;
	for (int i = 2; i < argc; i++)
	{
		if (chown(argv[i], uid, gid) == -1)
		{
			perror("chown");
			ret = 1;
		}
	}

	return ret;
}
