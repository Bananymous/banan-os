#include <BAN/Assert.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/weak_alias.h>

char** __environ;
weak_alias(__environ, environ);

static bool s_environ_malloced = false;
static size_t s_environ_count = 0;          // only valid when s_environ_malloced == true
static uint8_t* s_environ_bitmap = nullptr; // if bit i is set, environ[i] has to be freed

static int malloc_environ()
{
	ASSERT(!s_environ_malloced);

	size_t environ_count = 0;
	while (environ[environ_count])
		environ_count++;

	const size_t bitmap_size = (environ_count + 7) / 8;
	auto* new_bitmap = static_cast<uint8_t*>(malloc(bitmap_size));
	if (new_bitmap == nullptr)
		return -1;
	memset(new_bitmap, 0, bitmap_size);

	char** new_environ = static_cast<char**>(malloc((environ_count + 1) * sizeof(char*)));
	if (new_environ == nullptr)
	{
		free(new_bitmap);
		return -1;
	}

	for (size_t i = 0; i < environ_count; i++)
		new_environ[i] = environ[i];
	new_environ[environ_count] = nullptr;

	environ = new_environ;
	s_environ_malloced = true;
	s_environ_count = environ_count;
	s_environ_bitmap = new_bitmap;

	return 0;
}

static int putenv_impl(char* string, bool malloced)
{
	if (!s_environ_malloced && malloc_environ() == -1)
		return -1;

	const char* eq_addr = strchr(string, '=');
	if (eq_addr == nullptr)
	{
		errno = EINVAL;
		return -1;
	}

	const size_t namelen = eq_addr - string;
	for (int i = 0; environ[i]; i++)
	{
		if (strncmp(environ[i], string, namelen + 1) == 0)
		{
			const size_t byte = i / 8;
			const uint8_t mask = 1 << (i % 8);

			if (s_environ_bitmap[byte] & mask)
				free(environ[i]);

			if (malloced)
				s_environ_bitmap[i / 8] |= mask;
			else
				s_environ_bitmap[i / 8] &= ~mask;

			environ[i] = string;
			return 0;
		}
	}

	if ((s_environ_count + 1) % 8 == 0)
	{
		const size_t bytes = (s_environ_count + 1) / 8;

		void* new_bitmap = realloc(s_environ_bitmap, bytes);
		if (new_bitmap == nullptr)
			return -1;

		s_environ_bitmap = static_cast<uint8_t*>(new_bitmap);
		s_environ_bitmap[bytes - 1] = 0;
	}

	void* new_environ = realloc(environ, sizeof(char*) * (s_environ_count + 2));
	if (new_environ == nullptr)
		return -1;

	environ = static_cast<char**>(new_environ);
	environ[s_environ_count] = string;
	environ[s_environ_count + 1] = nullptr;
	s_environ_count++;

	if (malloced)
	{
		const size_t byte = s_environ_count / 8;
		const size_t mask = 1 << (s_environ_count % 8);
		s_environ_bitmap[byte] |= mask;
	}

	return 0;
}

char* getenv(const char* name)
{
	if (environ == nullptr)
		return nullptr;
	const size_t namelen = strlen(name);
	for (int i = 0; environ[i]; i++)
		if (strncmp(name, environ[i], namelen) == 0)
			if (environ[i][namelen] == '=')
				return environ[i] + namelen + 1;
	return nullptr;
}

int setenv(const char* name, const char* val, int overwrite)
{
	if (name == nullptr || !name[0] || strchr(name, '='))
	{
		errno = EINVAL;
		return -1;
	}

	if (!overwrite && getenv(name))
		return 0;

	const size_t namelen = strlen(name);
	const size_t vallen = strlen(val);

	char* string = (char*)malloc(namelen + vallen + 2);
	memcpy(string, name, namelen);
	string[namelen] = '=';
	memcpy(string + namelen + 1, val, vallen);
	string[namelen + vallen + 1] = '\0';

	return putenv_impl(string, true);
}

int unsetenv(const char* name)
{
	if (name == nullptr || !name[0] || strchr(name, '='))
	{
		errno = EINVAL;
		return -1;
	}

	const size_t namelen = strlen(name);

	size_t i = 0;
	for (; environ[i]; i++)
	{
		if (strncmp(environ[i], name, namelen) || environ[i][namelen] != '=')
			continue;
		if (!s_environ_malloced)
			break;
		const size_t byte = i / 8;
		const size_t mask = 1 << (i % 8);
		if (s_environ_bitmap[byte] & mask)
			free(environ[i]);
		s_environ_count--;
		break;
	}

	for (; environ[i] && environ[i + 1]; i++)
	{
		environ[i] = environ[i + 1];
		if (!s_environ_malloced)
			continue;

		const size_t cbyte = i / 8;
		const size_t cmask = 1 << (i % 8);

		const size_t nbyte = (i + 1) / 8;
		const size_t nmask = 1 << ((i + 1) % 8);

		if (s_environ_bitmap[nbyte] & nmask)
			s_environ_bitmap[cbyte] |= cmask;
		else
			s_environ_bitmap[cbyte] &= ~cmask;
	}

	if (environ[i])
	{
		environ[i] = nullptr;

		if (s_environ_malloced)
		{
			const size_t byte = i / 8;
			const size_t mask = 1 << (i % 8);
			s_environ_bitmap[byte] &= ~mask;
		}
	}

	return 0;
}

int putenv(char* string)
{
	if (string == nullptr || !string[0])
	{
		errno = EINVAL;
		return -1;
	}

	return putenv_impl(string, false);
}
