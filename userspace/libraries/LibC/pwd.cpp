#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE* s_pwent_fp = nullptr;
static passwd s_pwent_struct;

static char* s_pwent_buffer = nullptr;
static size_t s_pwent_buffer_size = 0;

void endpwent(void)
{
	if (s_pwent_fp)
		fclose(s_pwent_fp);
	s_pwent_fp = nullptr;

	if (s_pwent_buffer)
		free(s_pwent_buffer);
	s_pwent_buffer = nullptr;
}

void setpwent(void)
{
	if (!s_pwent_fp)
		return;
	fseek(s_pwent_fp, 0, SEEK_SET);
}

static int getpwent_impl(FILE* fp, struct passwd* pwd, char* buffer, size_t bufsize, struct passwd** result)
{
	for (;;)
	{
		if (fgets(buffer, bufsize, fp) == nullptr)
		{
			if (ferror(fp))
				return errno;
			*result = nullptr;
			return 0;
		}

		const size_t line_len = strlen(buffer);
		if (line_len == 0)
			continue;

		if (buffer[line_len - 1] == '\n')
			buffer[line_len - 1] = '\0';
		else if (!feof(fp))
			return (errno = ERANGE);

#define GET_STRING() ({ \
			ptr = strchr(ptr, ':'); \
			if (ptr == nullptr) \
				continue; \
			*ptr++ = '\0'; \
		})

#define GET_INT() ({ \
				if (!isdigit(*ptr)) \
					continue; \
				long val = 0; \
				while (isdigit(*ptr)) \
					val = (val * 10) + (*ptr++ - '0'); \
				if (*ptr != ':') \
					continue; \
				*ptr++ = '\0'; \
				val; \
			})

		char* ptr = buffer;

		pwd->pw_name = ptr;
		GET_STRING();

		pwd->pw_passwd = ptr;
		GET_STRING();

		pwd->pw_uid = GET_INT();

		pwd->pw_gid = GET_INT();

		pwd->pw_gecos = ptr;
		GET_STRING();

		pwd->pw_dir = ptr;
		GET_STRING();

		pwd->pw_shell = ptr;

		*result = pwd;
		return 0;
	}
}

struct passwd* getpwent(void)
{
	if (s_pwent_fp == nullptr)
	{
		s_pwent_fp = fopen("/etc/passwd", "r");
		if (s_pwent_fp == nullptr)
			return nullptr;
	}

	if (s_pwent_buffer == nullptr)
	{
		long size = sysconf(_SC_GETPW_R_SIZE_MAX);
		if (size == -1)
			size = 512;

		s_pwent_buffer = static_cast<char*>(malloc(size));
		if (s_pwent_buffer == nullptr)
			return nullptr;
		s_pwent_buffer_size = size;
	}

	const off_t old_offset = ftello(s_pwent_fp);

	passwd* result;
	for (;;)
	{
		const int error = getpwent_impl(s_pwent_fp, &s_pwent_struct, s_pwent_buffer, s_pwent_buffer_size, &result);
		if (error == 0)
			break;
		fseeko(s_pwent_fp, old_offset, SEEK_SET);
		if (error != ERANGE)
			return nullptr;

		const size_t new_size = s_pwent_buffer_size * 2;
		char* new_buffer = static_cast<char*>(realloc(s_pwent_buffer, new_size));
		if (new_buffer == nullptr)
			return nullptr;

		s_pwent_buffer = new_buffer;
		s_pwent_buffer_size = new_size;
	}

	return result;
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

int getpwuid_r(uid_t uid, struct passwd* pwd, char* buffer, size_t bufsize, struct passwd** result)
{
	FILE* fp = fopen("/etc/passwd", "r");
	if (fp == nullptr)
		return errno;

	int ret = 0;
	for (;;)
	{
		if ((ret = getpwent_impl(fp, pwd, buffer, bufsize, result)))
			break;
		if (*result == nullptr)
			break;
		if (pwd->pw_uid == uid)
			break;
	}

	fclose(fp);
	return ret;
}

int getpwnam_r(const char* name, struct passwd* pwd, char* buffer, size_t bufsize, struct passwd** result)
{
	FILE* fp = fopen("/etc/passwd", "r");
	if (fp == nullptr)
		return errno;

	int ret = 0;
	for (;;)
	{
		if ((ret = getpwent_impl(fp, pwd, buffer, bufsize, result)))
			break;
		if (*result == nullptr)
			break;
		if (strcmp(pwd->pw_name, name) == 0)
			break;
	}

	fclose(fp);
	return ret;
}
