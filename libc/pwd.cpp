#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE* s_pwent_fp = nullptr;
static passwd s_pwent_struct;

#define TRY_LIBC(expr, ret) ({ auto&& e = expr; if (e.is_error()) { errno = e.error().get_error_code(); return ret; } e.release_value(); })

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
	
	BAN::String line;
	while (true)
	{
		char buffer[128];
		if (!fgets(buffer, sizeof(buffer), s_pwent_fp))
			return nullptr;
		TRY_LIBC(line.append(buffer), nullptr);

		if (line.back() == '\n')
		{
			line.pop_back();
			break;
		}
	}

	auto parts = TRY_LIBC(line.sv().split(':', true), nullptr);
	ASSERT(parts.size() == 7);

	ASSERT(1 <= parts[2].size() && parts[2].size() <= 9);
	for (char c : parts[2])
		ASSERT(isdigit(c));
	ASSERT(1 <= parts[3].size() && parts[3].size() <= 9);
	for (char c : parts[3])
		ASSERT(isdigit(c));

	s_pwent_struct.pw_uid = atoi(parts[2].data());
	s_pwent_struct.pw_gid = atoi(parts[3].data());

	s_pwent_struct.pw_name = (char*)malloc(parts[0].size() + 1);
	if (!s_pwent_struct.pw_name)
		return nullptr;
	memcpy(s_pwent_struct.pw_name, parts[0].data(), parts[0].size());
	s_pwent_struct.pw_name[parts[0].size()] = '\0';

	s_pwent_struct.pw_dir = (char*)malloc(parts[5].size() + 1);
	if (!s_pwent_struct.pw_dir)
		return nullptr;
	memcpy(s_pwent_struct.pw_dir, parts[5].data(), parts[5].size());
	s_pwent_struct.pw_dir[parts[5].size()] = '\0';

	s_pwent_struct.pw_shell = (char*)malloc(parts[6].size() + 1);
	if (!s_pwent_struct.pw_shell)
		return nullptr;
	memcpy(s_pwent_struct.pw_shell, parts[6].data(), parts[6].size());
	s_pwent_struct.pw_shell[parts[6].size()] = '\0';

	return &s_pwent_struct;
}

struct passwd* getpwnam(const char* name)
{
	passwd* pwd;
	setpwent();
	while (pwd = getpwent())
		if (strcmp(pwd->pw_name, name) == 0)
			return pwd;
	return nullptr;
}

struct passwd* getpwuid(uid_t uid)
{
	passwd* pwd;
	setpwent();
	while (pwd = getpwent())
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