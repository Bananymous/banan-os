#include <BAN/Assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern "C" void _fini();

void abort(void)
{
	ASSERT_NOT_REACHED();
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

char* getenv(const char*)
{
	return nullptr;
}

void* malloc(size_t)
{
	return nullptr;	
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

void free(void*)
{

}
