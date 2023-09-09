#include <BAN/Assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

extern "C" char** environ;

extern "C" void _fini();

void abort(void)
{
	fflush(nullptr);
	fprintf(stderr, "abort()\n");
	exit(1);
}

void exit(int status)
{
	fflush(nullptr);
	_fini();
	_exit(status);
	ASSERT_NOT_REACHED();
}

int abs(int val)
{
	return val < 0 ? -val : val;
}

int atexit(void(*)(void))
{
	return -1;
}

int atoi(const char* str)
{
	while (isspace(*str))
		str++;

	bool negative = (*str == '-');

	if (*str == '-' || *str == '+')
		str++;

	int res = 0;
	while (isdigit(*str))
	{
		res = (res * 10) + (*str - '0');
		str++;
	}

	return negative ? -res : res;
}

char* getenv(const char* name)
{
	if (environ == nullptr)
		return nullptr;
	size_t len = strlen(name);
	for (int i = 0; environ[i]; i++)
		if (strncmp(name, environ[i], len) == 0)
			if (environ[i][len] == '=')
				return environ[i] + len + 1;
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

	size_t namelen = strlen(name);
	size_t vallen = strlen(val);

	char* string = (char*)malloc(namelen + vallen + 2);
	memcpy(string, name, namelen);
	string[namelen] = '=';
	memcpy(string + namelen + 1, val, vallen);
	string[namelen + vallen + 1] = '\0';

	return putenv(string);
}

int unsetenv(const char* name)
{
	if (name == nullptr || !name[0] || strchr(name, '='))
	{
		errno = EINVAL;
		return -1;
	}

	size_t len = strlen(name);

	bool found = false;
	for (int i = 0; environ[i]; i++)
	{
		if (!found && strncmp(environ[i], name, len) == 0 && environ[i][len] == '=')
		{
			free(environ[i]);
			found = true;
		}
		if (found)
			environ[i] = environ[i + 1];
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

	int cnt = 0;
	for (int i = 0; string[i]; i++)
		if (string[i] == '=')
			cnt++;
	if (cnt != 1)
	{
		errno = EINVAL;
		return -1;
	}

	int namelen = strchr(string, '=') - string;
	for (int i = 0; environ[i]; i++)
	{
		if (strncmp(environ[i], string, namelen + 1) == 0)
		{
			free(environ[i]);
			environ[i] = string;
			return 0;
		}
	}

	int env_count = 0;
	while (environ[env_count])
		env_count++;

	char** new_envp = (char**)malloc(sizeof(char*) * (env_count + 2));
	if (new_envp == nullptr)
	{
		errno = ENOMEM;
		return -1;
	}

	for (int i = 0; i < env_count; i++)
		new_envp[i] = environ[i];
	new_envp[env_count] = string;
	new_envp[env_count + 1] = nullptr;

	free(environ);
	environ = new_envp;

	if (syscall(SYS_SETENVP, environ) == -1)
		return -1;
	return 0;
}

void* malloc(size_t bytes)
{
	long res = syscall(SYS_ALLOC, bytes);
	if (res < 0)
		return nullptr;
	return (void*)res;
}

void* calloc(size_t nmemb, size_t size)
{
	if (nmemb * size < nmemb)
		return nullptr;
	void* ptr = malloc(nmemb * size);
	if (ptr == nullptr)
		return nullptr;
	memset(ptr, 0, nmemb * size);
	return ptr;
}

void* realloc(void* ptr, size_t size)
{
	if (ptr == nullptr)
		return malloc(size);
	long ret = syscall(SYS_REALLOC, ptr, size);
	if (ret == -1)
		return nullptr;
	return (void*)ret;
}

void free(void* ptr)
{
	if (ptr == nullptr)
		return;
	syscall(SYS_FREE, ptr);
}

// Constants and algorithm from https://en.wikipedia.org/wiki/Permuted_congruential_generator

static uint64_t s_rand_state = 0x4d595df4d0f33173;
static constexpr uint64_t s_rand_multiplier = 6364136223846793005;
static constexpr uint64_t s_rand_increment = 1442695040888963407;

static constexpr uint32_t rotr32(uint32_t x, unsigned r)
{
	return x >> r | x << (-r & 31);
}

int rand(void)
{
	uint64_t x = s_rand_state;
	unsigned count = (unsigned)(x >> 59);

	s_rand_state = x * s_rand_multiplier + s_rand_increment;
	x ^= x >> 18;

	return rotr32(x >> 27, count) % RAND_MAX;
}

void srand(unsigned int seed)
{
	s_rand_state = seed + s_rand_increment;
	(void)rand();
}