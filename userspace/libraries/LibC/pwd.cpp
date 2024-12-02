#include <BAN/Assert.h>

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE* s_pwent_fp = nullptr;
static passwd s_pwent_struct;

static bool open_pwent()
{
	if (s_pwent_fp)
		return true;
	s_pwent_fp = fopen("/etc/passwd", "r");
	return s_pwent_fp;
}

static void clear_pwent(passwd& passwd)
{
	if (passwd.pw_name)
		free(passwd.pw_name);
	passwd.pw_name = nullptr;
	if (passwd.pw_dir)
		free(passwd.pw_dir);
	passwd.pw_dir = nullptr;
	if (passwd.pw_shell)
		free(passwd.pw_shell);
	passwd.pw_shell = nullptr;
}

void endpwent(void)
{
	if (!s_pwent_fp)
		return;
	fclose(s_pwent_fp);
	s_pwent_fp = nullptr;
	clear_pwent(s_pwent_struct);
}

struct passwd* getpwent(void)
{
	if (!s_pwent_fp)
		if (!open_pwent())
			return nullptr;
	clear_pwent(s_pwent_struct);

	static char buffer[4096];
	if (!fgets(buffer, sizeof(buffer), s_pwent_fp))
		return nullptr;

	size_t buffer_len = strlen(buffer);

	ASSERT(buffer[buffer_len - 1] == '\n');
	buffer[buffer_len - 1] = '\0';
	buffer_len--;

	const char* ptr = buffer;
	for (int i = 0; i < 7; i++)
	{
		char* end = strchr(ptr, ':');
		ASSERT((i < 6) ? end != nullptr : end == nullptr);
		if (!end)
			end = buffer + buffer_len;
		*end = '\0';

		const size_t field_len = end - ptr;

		switch (i)
		{
			case 0:
				s_pwent_struct.pw_name = (char*)malloc(field_len + 1);
				if (!s_pwent_struct.pw_name)
					return nullptr;
				strcpy(s_pwent_struct.pw_name, ptr);
				break;
			case 1:
				break;
			case 2:
				ASSERT(1 <= field_len && field_len <= 9);
				for (size_t j = 0; j < field_len; j++)
					ASSERT(isdigit(ptr[j]));
				s_pwent_struct.pw_uid = atoi(ptr);
				break;
			case 3:
				ASSERT(1 <= field_len && field_len <= 9);
				for (size_t j = 0; j < field_len; j++)
					ASSERT(isdigit(ptr[j]));
				s_pwent_struct.pw_gid = atoi(ptr);
				break;
			case 4:
				break;
			case 5:
				s_pwent_struct.pw_dir = (char*)malloc(field_len + 1);
				if (!s_pwent_struct.pw_dir)
					return nullptr;
				strcpy(s_pwent_struct.pw_dir, ptr);
				break;
			case 6:
				s_pwent_struct.pw_shell = (char*)malloc(field_len + 1);
				if (!s_pwent_struct.pw_shell)
					return nullptr;
				strcpy(s_pwent_struct.pw_shell, ptr);
				break;
		}

		ptr = end + 1;
	}

	return &s_pwent_struct;
}

struct passwd* getpwnam(const char* name)
{
	passwd* pwd;
	setpwent();
	while ((pwd = getpwent()))
		if (strcmp(pwd->pw_name, name) == 0)
			return pwd;
	return nullptr;
}

struct passwd* getpwuid(uid_t uid)
{
	passwd* pwd;
	setpwent();
	while ((pwd = getpwent()))
		if (pwd->pw_uid == uid)
			return pwd;
	return nullptr;
}

void setpwent(void)
{
	if (!s_pwent_fp)
		return;
	fseek(s_pwent_fp, 0, SEEK_SET);
}
