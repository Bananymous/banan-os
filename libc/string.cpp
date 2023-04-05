#include <string.h>
#include <errno.h>

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
	static char buffer[100];
	buffer[0] = 0;

	switch (error)
	{
	case ENOMEM:
		strcpy(buffer, "Cannot allocate memory");
		break;
	case EINVAL:
		strcpy(buffer, "Invalid argument");
		break;
	case EISDIR:
		strcpy(buffer, "Is a directory");
		break;
	case ENOTDIR:
		strcpy(buffer, "Not a directory");
		break;
	case ENOENT:
		strcpy(buffer, "No such file or directory");
		break;
	case EIO:
		strcpy(buffer, "Input/output error");
		break;
	case ENOTSUP:
		strcpy(buffer, "Operation not supported");
		break;
	case EBADF:
		strcpy(buffer, "Bad file descriptor");
		break;
	case EEXISTS:
		strcpy(buffer, "File exists");
		break;
	case ENOTEMPTY:
		strcpy(buffer, "Directory not empty");
		break;
	case ENAMETOOLONG:
		strcpy(buffer, "Filename too long");
		break;
	case ENOBUFS:
		strcpy(buffer, "No buffer space available");
		break;
	case ENOTTY:
		strcpy(buffer, "Inappropriate I/O control operation");
		break;
	default:
		{
			// FIXME: sprintf
			//sprintf(buffer, "Unknown error %d", error);
			strcpy(buffer, "Unknown error");
			errno = EINVAL;
			break;
		}
	}

	return buffer;
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
