#include <errno.h>
#include <stdio.h>
#include <string.h>

int errno = 0;

int memcmp(const void* s1, const void* s2, size_t n)
{
	const unsigned char* a = static_cast<const unsigned char*>(s1);
	const unsigned char* b = static_cast<const unsigned char*>(s2);

	for (size_t i = 0; i < n; i++)
		if (a[i] != b[i])
			return a[i] - b[i];

	return 0;
}

void* memcpy(void* __restrict__ dstp, const void* __restrict__ srcp, size_t n)
{
	unsigned char* dst = static_cast<unsigned char*>(dstp);
	const unsigned char* src = static_cast<const unsigned char*>(srcp);
	for (size_t i = 0; i < n; i++)
		dst[i] = src[i];
	return dstp;
}

void* memmove(void* destp, const void* srcp, size_t n)
{
	unsigned char* dest = static_cast<unsigned char*>(destp);
	const unsigned char* src = static_cast<const unsigned char*>(srcp);

	if (dest < src)
	{
		for (size_t i = 0; i < n; i++)
			dest[i] = src[i];
	}
	else
	{
		for (size_t i = 1; i <= n; i++)
			dest[n - i] = src[n - i];
	}

	return destp;
}

void* memset(void* s, int c, size_t n)
{
	unsigned char* p = static_cast<unsigned char*>(s);
	for (size_t i = 0; i < n; i++)
		p[i] = c;
	return s;
}

int strcmp(const char* s1, const char* s2)
{
	const unsigned char* u1 = (unsigned char*)s1;
	const unsigned char* u2 = (unsigned char*)s2;
	for (; *u1 && *u2; u1++, u2++)
		if (*u1 != *u2)
			break;
	return *u1 - *u2;
}

char* strcpy(char* __restrict__ dest, const char* __restrict__ src)
{
	size_t i;
	for (i = 0; src[i]; i++)
		dest[i] = src[i];
	dest[i] = '\0';
	return dest;
}

char* strcat(char* __restrict__ dest, const char* __restrict__ src)
{
	strcpy(dest + strlen(src), src);
	return dest;
}

char* strerror(int error)
{
	static char buffer[1024];
	buffer[0] = 0;
	strcpy(buffer, strerrordesc_np(error));
	return buffer;
}

const char* strerrorname_np(int error)
{
	switch (error)
	{
		case 0:				return "NOERROR";
		case ENOMEM:		return "ENOMEM";
		case EINVAL:		return "EINVAL";
		case EISDIR:		return "EISDIR";
		case ENOTDIR:		return "ENOTDIR";
		case ENOENT:		return "ENOENT";
		case EIO:			return "EIO";
		case ENOTSUP:		return "ENOTSUP";
		case EBADF:			return "EBADF";
		case EEXISTS:		return "EEXISTS";
		case ENOTEMPTY:		return "ENOTEMPTY";
		case ENAMETOOLONG:	return "ENAMETOOLONG";
		case ENOBUFS:		return "ENOBUFS";
		case ENOTTY:		return "ENOTTY";
		case ENOTBLK:		return "ENOTBLK";
		case EMFILE:		return "EMFILE";
		case ENOSYS:		return "ENOSYS";
	}

	errno = EINVAL;
	return "EUNKNOWN";
}

const char* strerrordesc_np(int error)
{
	switch (error)
	{
		case 0:				return "Success";
		case ENOMEM:		return "Cannot allocate memory";
		case EINVAL:		return "Invalid argument";
		case EISDIR:		return "Is a directory";
		case ENOTDIR:		return "Not a directory";
		case ENOENT:		return "No such file or directory";
		case EIO:			return "Input/output error";
		case ENOTSUP:		return "Operation not supported";
		case EBADF:			return "Bad file descriptor";
		case EEXISTS:		return "File exists";
		case ENOTEMPTY:		return "Directory not empty";
		case ENAMETOOLONG:	return "Filename too long";
		case ENOBUFS:		return "No buffer space available";
		case ENOTTY:		return "Inappropriate I/O control operation";
		case ENOTBLK:		return "Block device required";
		case EMFILE:		return "File descriptor value too large";
		case ENOSYS:		return "Function not implemented";
	}

	errno = EINVAL;
	return "Unknown error";
}


size_t strlen(const char* str)
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

int strncmp(const char* s1, const char* s2, size_t n)
{
	const unsigned char* u1 = (unsigned char*)s1;
	const unsigned char* u2 = (unsigned char*)s2;
	for (; --n && *u1 && *u2; u1++, u2++)
		if (*u1 != *u2)
			break;
	return *u1 - *u2;
}

char* strchr(const char* str, int c)
{
	while (*str)
	{
		if (*str == c)
			return (char*)str;
		str++;
	}
	return (*str == c) ? (char*)str : nullptr;
}

char* strncpy(char* __restrict__ dest, const char* __restrict__ src, size_t n)
{
	size_t i;
	for (i = 0; src[i] && i < n; i++)
		dest[i] = src[i];
	for (; i < n; i++)
		dest[i] = '\0';
	return dest;
}

char* strstr(const char* haystack, const char* needle)
{
	for (size_t i = 0; haystack[i]; i++)
		if (memcmp(haystack + i, needle, strlen(needle)) == 0)
			return (char*)haystack + i;
	return NULL;
}
