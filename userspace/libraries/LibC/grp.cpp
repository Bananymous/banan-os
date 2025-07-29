#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE* s_grent_fp = nullptr;
static group s_grent_struct;

static char* s_grent_buffer = nullptr;
static size_t s_grent_buffer_size = 0;

void endgrent(void)
{
	if (s_grent_fp)
		fclose(s_grent_fp);
	s_grent_fp = nullptr;

	if (s_grent_buffer)
		free(s_grent_buffer);
	s_grent_buffer = nullptr;
}

void setgrent(void)
{
	if (!s_grent_fp)
		return;
	fseek(s_grent_fp, 0, SEEK_SET);
}

static int getgrent_impl(FILE* fp, struct group* grp, char* buffer, size_t bufsize, struct group** result)
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

		grp->gr_name = ptr;
		GET_STRING();

		grp->gr_passwd = ptr;
		GET_STRING();

		grp->gr_gid = GET_INT();

		size_t offset = line_len + 1;
		if (auto rem = offset % alignof(char*))
			offset += alignof(char*) - rem;
		grp->gr_mem = reinterpret_cast<char**>(buffer + offset);

		const size_t mem_max = (bufsize - offset) / sizeof(char*);
		size_t mem_idx = 0;

		while (*ptr && mem_idx + 1 <= mem_max)
		{
			grp->gr_mem[mem_idx++] = ptr;

			ptr = strchrnul(ptr, ',');
			if (*ptr == ',')
				*ptr++ = '\0';
		}

		if (mem_idx + 1 > mem_max)
			return (errno = ERANGE);

		grp->gr_mem[mem_idx] = nullptr;
		*result = grp;
		return 0;
	}
}

struct group* getgrent(void)
{
	if (s_grent_fp == nullptr)
	{
		s_grent_fp = fopen("/etc/group", "r");
		if (s_grent_fp == nullptr)
			return nullptr;
	}

	if (s_grent_buffer == nullptr)
	{
		long size = sysconf(_SC_GETGR_R_SIZE_MAX);
		if (size == -1)
			size = 512;

		s_grent_buffer = static_cast<char*>(malloc(size));
		if (s_grent_buffer == nullptr)
			return nullptr;
		s_grent_buffer_size = size;
	}

	const off_t old_offset = ftello(s_grent_fp);

	group* result;
	for (;;)
	{
		const int error = getgrent_impl(s_grent_fp, &s_grent_struct, s_grent_buffer, s_grent_buffer_size, &result);
		if (error == 0)
			break;
		fseeko(s_grent_fp, old_offset, SEEK_SET);
		if (error != ERANGE)
			return nullptr;

		const size_t new_size = s_grent_buffer_size * 2;
		char* new_buffer = static_cast<char*>(realloc(s_grent_buffer, new_size));
		if (new_buffer == nullptr)
			return nullptr;

		s_grent_buffer = new_buffer;
		s_grent_buffer_size = new_size;
	}

	return result;
}

struct group* getgrgid(gid_t gid)
{
	group* grp;
	setgrent();
	while ((grp = getgrent()))
		if (grp->gr_gid == gid)
			return grp;
	return nullptr;
}

struct group* getgrnam(const char* name)
{
	group* grp;
	setgrent();
	while ((grp = getgrent()))
		if (strcmp(grp->gr_name, name) == 0)
			return grp;
	return nullptr;
}

int getgrgid_r(gid_t gid, struct group* grp, char* buffer, size_t bufsize, struct group** result)
{
	FILE* fp = fopen("/etc/group", "r");
	if (fp == nullptr)
		return errno;

	int ret = 0;
	for (;;)
	{
		if ((ret = getgrent_impl(fp, grp, buffer, bufsize, result)))
			break;
		if (*result == nullptr)
			break;
		if (grp->gr_gid == gid)
			break;
	}

	fclose(fp);
	return ret;
}

int getgrnam_r(const char* name, struct group* grp, char* buffer, size_t bufsize, struct group** result)
{
	FILE* fp = fopen("/etc/group", "r");
	if (fp == nullptr)
		return errno;

	int ret = 0;
	for (;;)
	{
		if ((ret = getgrent_impl(fp, grp, buffer, bufsize, result)))
			break;
		if (*result == nullptr)
			break;
		if (strcmp(grp->gr_name, name) == 0)
			break;
	}

	fclose(fp);
	return ret;
}
