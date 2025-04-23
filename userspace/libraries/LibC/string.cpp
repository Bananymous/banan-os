#include <BAN/Assert.h>
#include <BAN/UTF8.h>

#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/weak_alias.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize "no-tree-loop-distribute-patterns"
#endif

extern "C" void* _memccpy(void* __restrict s1, const void* __restrict s2, int c, size_t n)
{
	unsigned char* dst = static_cast<unsigned char*>(s1);
	const unsigned char* src = static_cast<const unsigned char*>(s2);
	for (size_t i = 0; i < n; i++)
		if ((dst[i] = src[i]) == c)
			return dst + i + 1;
	return nullptr;
}
weak_alias(_memccpy, memccpy);

extern "C" void* _memchr(const void* s, int c, size_t n)
{
	const unsigned char* u = static_cast<const unsigned char*>(s);
	for (size_t i = 0; i < n; i++)
		if (u[i] == c)
			return const_cast<unsigned char*>(u + i);
	return nullptr;
}
weak_alias(_memchr, memchr);

extern "C" int _memcmp(const void* s1, const void* s2, size_t n)
{
	const unsigned char* a = static_cast<const unsigned char*>(s1);
	const unsigned char* b = static_cast<const unsigned char*>(s2);
	for (size_t i = 0; i < n; i++)
		if (a[i] != b[i])
			return a[i] - b[i];
	return 0;
}
weak_alias(_memcmp, memcmp);

extern "C" void* _memcpy(void* __restrict__ dstp, const void* __restrict__ srcp, size_t n)
{
	unsigned char* dst = static_cast<unsigned char*>(dstp);
	const unsigned char* src = static_cast<const unsigned char*>(srcp);
	for (size_t i = 0; i < n; i++)
		dst[i] = src[i];
	return dstp;
}
weak_alias(_memcpy, memcpy);

extern "C" void* _memmove(void* destp, const void* srcp, size_t n)
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
weak_alias(_memmove, memmove);

extern "C" void* _memset(void* s, int c, size_t n)
{
	unsigned char* p = static_cast<unsigned char*>(s);
	for (size_t i = 0; i < n; i++)
		p[i] = c;
	return s;
}
weak_alias(_memset, memset);

extern "C" int _strcmp(const char* s1, const char* s2)
{
	const unsigned char* u1 = (unsigned char*)s1;
	const unsigned char* u2 = (unsigned char*)s2;
	for (; *u1 && *u2; u1++, u2++)
		if (*u1 != *u2)
			break;
	return *u1 - *u2;
}
weak_alias(_strcmp, strcmp);

extern "C" int _strncmp(const char* s1, const char* s2, size_t n)
{
	if (n == 0)
		return 0;
	const unsigned char* u1 = (unsigned char*)s1;
	const unsigned char* u2 = (unsigned char*)s2;
	for (; --n && *u1 && *u2; u1++, u2++)
		if (*u1 != *u2)
			break;
	return *u1 - *u2;
}
weak_alias(_strncmp, strncmp);

extern "C" char* _stpcpy(char* __restrict__ dest, const char* __restrict__ src)
{
	size_t i = 0;
	for (; src[i]; i++)
		dest[i] = src[i];
	dest[i] = '\0';
	return &dest[i];
}
weak_alias(_stpcpy, stpcpy);

extern "C" char* _stpncpy(char* __restrict__ dest, const char* __restrict__ src, size_t n)
{
	size_t i = 0;
	for (; src[i] && n; i++, n--)
		dest[i] = src[i];
	for (; n; i++, n--)
		dest[i] = '\0';
	return &dest[i];
}
weak_alias(_stpncpy, stpncpy);

char* strcpy(char* __restrict__ dest, const char* __restrict__ src)
{
	stpcpy(dest, src);
	return dest;
}

char* strncpy(char* __restrict__ dest, const char* __restrict__ src, size_t n)
{
	stpncpy(dest, src, n);
	return dest;
}

char* strcat(char* __restrict__ dest, const char* __restrict__ src)
{
	strcpy(dest + strlen(dest), src);
	return dest;
}

char* strncat(char* __restrict__ dest, const char* __restrict__ src, size_t n)
{
	dest += strlen(dest);
	while (*src && n--)
		*dest++ = *src++;
	*dest = '\0';
	return dest;
}

int strcoll(const char* s1, const char* s2)
{
	return strcoll_l(s1, s2, __getlocale(LC_COLLATE));
}

int strcoll_l(const char *s1, const char *s2, locale_t locale)
{
	switch (locale)
	{
		case LOCALE_INVALID:
			ASSERT_NOT_REACHED();
		case LOCALE_POSIX:
			return strcmp(s1, s2);
		case LOCALE_UTF8:
		{
			const unsigned char* u1 = (unsigned char*)s1;
			const unsigned char* u2 = (unsigned char*)s2;
			if (!*u1 || !*u2)
				return *u1 - *u2;

			wchar_t wc1, wc2;
			while (*u1 && *u2)
			{
				wc1 = BAN::UTF8::to_codepoint(u1);
				wc2 = BAN::UTF8::to_codepoint(u2);
				if (wc1 == (wchar_t)BAN::UTF8::invalid || wc2 == (wchar_t)BAN::UTF8::invalid)
				{
					errno = EINVAL;
					return -1;
				}
				if (wc1 != wc2)
					break;
				u1 += BAN::UTF8::byte_length(*u1);
				u2 += BAN::UTF8::byte_length(*u2);
			}
			return wc1 - wc2;
		}
	}
	ASSERT_NOT_REACHED();
}

char* strdup(const char* str)
{
	const size_t size = strlen(str);

	char* new_str = (char*)malloc(size + 1);
	if (new_str == nullptr)
		return nullptr;

	memcpy(new_str, str, size);
	new_str[size] = '\0';
	return new_str;
}

char* strndup(const char* str, size_t size)
{
	if (size_t len = strlen(str); len < size)
		size = len;

	char* new_str = (char*)malloc(size + 1);
	if (new_str == nullptr)
		return nullptr;

	memcpy(new_str, str, size);
	new_str[size] = '\0';
	return new_str;
}

extern "C" size_t _strlen(const char* str)
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}
weak_alias(_strlen, strlen);

extern "C" size_t _strnlen(const char* str, size_t maxlen)
{
	size_t len = 0;
	while (len < maxlen && str[len])
		len++;
	return len;
}
weak_alias(_strnlen, strnlen);

char* strchr(const char* str, int c)
{
	if (c == '\0')
		return const_cast<char*>(str + strlen(str));
	char* result = strchrnul(str, c);
	return *result ? result : nullptr;
}

char* strchrnul(const char* str, int c)
{
	while (*str)
	{
		if (*str == (char)c)
			return (char*)str;
		str++;
	}
	return const_cast<char*>(str);
}

char* strrchr(const char* str, int c)
{
	size_t len = strlen(str);
	while (len > 0)
	{
		if (str[len] == (char)c)
			return (char*)str + len;
		len--;
	}
	return (*str == c) ? (char*)str : nullptr;
}

char* strstr(const char* haystack, const char* needle)
{
	const size_t needle_len = strlen(needle);
	if (needle_len == 0)
		return const_cast<char*>(haystack);
	for (size_t i = 0; haystack[i]; i++)
		if (strncmp(haystack + i, needle, needle_len) == 0)
			return const_cast<char*>(haystack + i);
	return nullptr;
}

#define CHAR_UCHAR(ch) \
	static_cast<unsigned char>(ch)

#define CHAR_BITMASK(str) \
	uint32_t bitmask[0x100 / 32] {}; \
	for (size_t i = 0; str[i]; i++) \
		bitmask[CHAR_UCHAR(str[i]) / 32] |= (1 << (CHAR_UCHAR(str[i]) % 32))

#define CHAR_BITMASK_TEST(ch) \
	(bitmask[CHAR_UCHAR(ch) / 32] & (1 << (CHAR_UCHAR(ch) % 32)))

char* strpbrk(const char* s1, const char* s2)
{
	CHAR_BITMASK(s2);
	for (size_t i = 0; s1[i]; i++)
		if (CHAR_BITMASK_TEST(s1[i]))
			return const_cast<char*>(&s1[i]);
	return nullptr;
}

size_t strspn(const char* s1, const char* s2)
{
	CHAR_BITMASK(s2);
	for (size_t i = 0;; i++)
		if (s1[i] == '\0' || !CHAR_BITMASK_TEST(s1[i]))
			return i;
}

size_t strcspn(const char* s1, const char* s2)
{
	CHAR_BITMASK(s2);
	for (size_t i = 0;; i++)
		if (s1[i] == '\0' || CHAR_BITMASK_TEST(s1[i]))
			return i;
}

char* strtok(char* __restrict s, const char* __restrict sep)
{
	static char* state = nullptr;
	return strtok_r(s, sep, &state);
}

char* strtok_r(char* __restrict str, const char* __restrict sep, char** __restrict state)
{
	CHAR_BITMASK(sep);

	if (str)
		*state = str;

	if (*state == nullptr)
		return nullptr;

	str = *state;

	for (; *str; str++)
		if (!CHAR_BITMASK_TEST(*str))
			break;

	if (*str == '\0')
	{
		*state = nullptr;
		return nullptr;
	}

	for (size_t i = 0; str[i]; i++)
	{
		if (!CHAR_BITMASK_TEST(str[i]))
			continue;
		str[i] = '\0';
		*state = str + i + 1;
		return str;
	}

	*state = nullptr;
	return str;
}

#undef CHAR_UCHAR
#undef CHAR_BITMASK
#undef CHAR_BITMASK_TEST

char* strsignal(int signum)
{
	static char buffer[128];
	switch (signum)
	{
		case SIGABRT:	strcpy(buffer, "Process abort signal."); break;
		case SIGALRM:	strcpy(buffer, "Alarm clock."); break;
		case SIGBUS:	strcpy(buffer, "Access to an undefined portion of a memory object."); break;
		case SIGCHLD:	strcpy(buffer, "Child process terminated, stopped, or continued."); break;
		case SIGCONT:	strcpy(buffer, "Continue executing, if stopped."); break;
		case SIGFPE:	strcpy(buffer, "Erroneous arithmetic operation."); break;
		case SIGHUP:	strcpy(buffer, "Hangup."); break;
		case SIGILL:	strcpy(buffer, "Illegal instruction."); break;
		case SIGINT:	strcpy(buffer, "Terminal interrupt signal."); break;
		case SIGKILL:	strcpy(buffer, "Kill (cannot be caught or ignored)."); break;
		case SIGPIPE:	strcpy(buffer, "Write on a pipe with no one to read it."); break;
		case SIGQUIT:	strcpy(buffer, "Terminal quit signal."); break;
		case SIGSEGV:	strcpy(buffer, "Invalid memory reference."); break;
		case SIGSTOP:	strcpy(buffer, "Stop executing (cannot be caught or ignored)."); break;
		case SIGTERM:	strcpy(buffer, "Termination signal."); break;
		case SIGTSTP:	strcpy(buffer, "Terminal stop signal."); break;
		case SIGTTIN:	strcpy(buffer, "Background process attempting read."); break;
		case SIGTTOU:	strcpy(buffer, "Background process attempting write."); break;
		case SIGUSR1:	strcpy(buffer, "User-defined signal 1."); break;
		case SIGUSR2:	strcpy(buffer, "User-defined signal 2."); break;
		case SIGPOLL:	strcpy(buffer, "Pollable event."); break;
		case SIGPROF:	strcpy(buffer, "Profiling timer expired."); break;
		case SIGSYS:	strcpy(buffer, "Bad system call."); break;
		case SIGTRAP:	strcpy(buffer, "Trace/breakpoint trap."); break;
		case SIGURG:	strcpy(buffer, "High bandwidth data is available at a socket."); break;
		case SIGVTALRM:	strcpy(buffer, "Virtual timer expired."); break;
		case SIGXCPU:	strcpy(buffer, "CPU time limit exceeded."); break;
		case SIGXFSZ:	strcpy(buffer, "File size limit exceeded."); break;
	}
	return buffer;
}

char* strerror(int error)
{
	static char buffer[128];
	if (const char* str = strerrordesc_np(error))
		strcpy(buffer, str);
	else
		sprintf(buffer, "Unknown error %d", error);
	return buffer;
}

int strerror_r(int error, char* strerrbuf, size_t buflen)
{
	const char* str = strerrordesc_np(error);
	if (!str)
		return EINVAL;
	if (strlen(str) + 1 > buflen)
		return ERANGE;
	strcpy(strerrbuf, str);
	return 0;
}

const char* strerrorname_np(int error)
{
	switch (error)
	{
		case 0:					return "NOERROR";
		case E2BIG:				return "E2BIG";
		case EACCES:			return "EACCES";
		case EADDRINUSE:		return "EADDRINUSE";
		case EADDRNOTAVAIL:		return "EADDRNOTAVAIL";
		case EAFNOSUPPORT:		return "EAFNOSUPPORT";
		case EAGAIN:			return "EAGAIN";
		case EALREADY:			return "EALREADY";
		case EBADF:				return "EBADF";
		case EBADMSG:			return "EBADMSG";
		case EBUSY:				return "EBUSY";
		case ECANCELED:			return "ECANCELED";
		case ECHILD:			return "ECHILD";
		case ECONNABORTED:		return "ECONNABORTED";
		case ECONNREFUSED:		return "ECONNREFUSED";
		case ECONNRESET:		return "ECONNRESET";
		case EDEADLK:			return "EDEADLK";
		case EDESTADDRREQ:		return "EDESTADDRREQ";
		case EDOM:				return "EDOM";
		case EDQUOT:			return "EDQUOT";
		case EEXIST:			return "EEXIST";
		case EFAULT:			return "EFAULT";
		case EFBIG:				return "EFBIG";
		case EHOSTUNREACH:		return "EHOSTUNREACH";
		case EIDRM:				return "EIDRM";
		case EILSEQ:			return "EILSEQ";
		case EINPROGRESS:		return "EINPROGRESS";
		case EINTR:				return "EINTR";
		case EINVAL:			return "EINVAL";
		case EIO:				return "EIO";
		case EISCONN:			return "EISCONN";
		case EISDIR:			return "EISDIR";
		case ELOOP:				return "ELOOP";
		case EMFILE:			return "EMFILE";
		case EMLINK:			return "EMLINK";
		case EMSGSIZE:			return "EMSGSIZE";
		case EMULTIHOP:			return "EMULTIHOP";
		case ENAMETOOLONG:		return "ENAMETOOLONG";
		case ENETDOWN:			return "ENETDOWN";
		case ENETRESET:			return "ENETRESET";
		case ENETUNREACH:		return "ENETUNREACH";
		case ENFILE:			return "ENFILE";
		case ENOBUFS:			return "ENOBUFS";
		case ENODATA:			return "ENODATA";
		case ENODEV:			return "ENODEV";
		case ENOENT:			return "ENOENT";
		case ENOEXEC:			return "ENOEXEC";
		case ENOLCK:			return "ENOLCK";
		case ENOLINK:			return "ENOLINK";
		case ENOMEM:			return "ENOMEM";
		case ENOMSG:			return "ENOMSG";
		case ENOPROTOOPT:		return "ENOPROTOOPT";
		case ENOSPC:			return "ENOSPC";
		case ENOSR:				return "ENOSR";
		case ENOSTR:			return "ENOSTR";
		case ENOSYS:			return "ENOSYS";
		case ENOTCONN:			return "ENOTCONN";
		case ENOTDIR:			return "ENOTDIR";
		case ENOTEMPTY:			return "ENOTEMPTY";
		case ENOTRECOVERABLE:	return "ENOTRECOVERABLE";
		case ENOTSOCK:			return "ENOTSOCK";
		case ENOTSUP:			return "ENOTSUP";
		case ENOTTY:			return "ENOTTY";
		case ENXIO:				return "ENXIO";
		case EOPNOTSUPP:		return "EOPNOTSUPP";
		case EOVERFLOW:			return "EOVERFLOW";
		case EOWNERDEAD:		return "EOWNERDEAD";
		case EPERM:				return "EPERM";
		case EPIPE:				return "EPIPE";
		case EPROTO:			return "EPROTO";
		case EPROTONOSUPPORT:	return "EPROTONOSUPPORT";
		case EPROTOTYPE:		return "EPROTOTYPE";
		case ERANGE:			return "ERANGE";
		case EROFS:				return "EROFS";
		case ESPIPE:			return "ESPIPE";
		case ESRCH:				return "ESRCH";
		case ESTALE:			return "ESTALE";
		case ETIME:				return "ETIME";
		case ETIMEDOUT:			return "ETIMEDOUT";
		case ETXTBSY:			return "ETXTBSY";
		case EWOULDBLOCK:		return "EWOULDBLOCK";
		case EXDEV:				return "EXDEV";
		case ENOTBLK:			return "ENOTBLK";
		case EUNKNOWN:			return "EUNKNOWN";
	}

	errno = EINVAL;
	return nullptr;
}

const char* strerrordesc_np(int error)
{
	switch (error)
	{
		case 0:					return "Success";
		case E2BIG:				return "Argument list too long.";
		case EACCES:			return "Permission denied.";
		case EADDRINUSE:		return "Address in use.";
		case EADDRNOTAVAIL:		return "Address not available.";
		case EAFNOSUPPORT:		return "Address family not supported.";
		case EAGAIN:			return "Resource unavailable, try again.";
		case EALREADY:			return "Connection already in progress.";
		case EBADF:				return "Bad file descriptor.";
		case EBADMSG:			return "Bad message.";
		case EBUSY:				return "Device or resource busy.";
		case ECANCELED:			return "Operation canceled.";
		case ECHILD:			return "No child processes.";
		case ECONNABORTED:		return "Connection aborted.";
		case ECONNREFUSED:		return "Connection refused.";
		case ECONNRESET:		return "Connection reset.";
		case EDEADLK:			return "Resource deadlock would occur.";
		case EDESTADDRREQ:		return "Destination address required.";
		case EDOM:				return "Mathematics argument out of domain of function.";
		case EDQUOT:			return "Reserved.";
		case EEXIST:			return "File exists.";
		case EFAULT:			return "Bad address.";
		case EFBIG:				return "File too large.";
		case EHOSTUNREACH:		return "Host is unreachable.";
		case EIDRM:				return "Identifier removed.";
		case EILSEQ:			return "Illegal byte sequence.";
		case EINPROGRESS:		return "Operation in progress.";
		case EINTR:				return "Interrupted function.";
		case EINVAL:			return "Invalid argument.";
		case EIO:				return "I/O error.";
		case EISCONN:			return "Socket is connected.";
		case EISDIR:			return "Is a directory.";
		case ELOOP:				return "Too many levels of symbolic links.";
		case EMFILE:			return "File descriptor value too large.";
		case EMLINK:			return "Too many links.";
		case EMSGSIZE:			return "Message too large.";
		case EMULTIHOP:			return "Reserved.";
		case ENAMETOOLONG:		return "Filename too long.";
		case ENETDOWN:			return "Network is down.";
		case ENETRESET:			return "Connection aborted by network.";
		case ENETUNREACH:		return "Network unreachable.";
		case ENFILE:			return "Too many files open in system.";
		case ENOBUFS:			return "No buffer space available.";
		case ENODATA:			return "No message is available on the STREAM head read queue.";
		case ENODEV:			return "No such device.";
		case ENOENT:			return "No such file or directory.";
		case ENOEXEC:			return "Executable file format error.";
		case ENOLCK:			return "No locks available.";
		case ENOLINK:			return "Reserved.";
		case ENOMEM:			return "Not enough space.";
		case ENOMSG:			return "No message of the desired type.";
		case ENOPROTOOPT:		return "Protocol not available.";
		case ENOSPC:			return "No space left on device.";
		case ENOSR:				return "No STREAM resources.";
		case ENOSTR:			return "Not a STREAM.";
		case ENOSYS:			return "Functionality not supported.";
		case ENOTCONN:			return "The socket is not connected.";
		case ENOTDIR:			return "Not a directory or a symbolic link to a directory.";
		case ENOTEMPTY:			return "Directory not empty.";
		case ENOTRECOVERABLE:	return "State not recoverable.";
		case ENOTSOCK:			return "Not a socket.";
		case ENOTSUP:			return "Not supported.";
		case ENOTTY:			return "Inappropriate I/O control operation.";
		case ENXIO:				return "No such device or address.";
		case EOPNOTSUPP:		return "Operation not supported on socket .";
		case EOVERFLOW:			return "Value too large to be stored in data type.";
		case EOWNERDEAD:		return "Previous owner died.";
		case EPERM:				return "Operation not permitted.";
		case EPIPE:				return "Broken pipe.";
		case EPROTO:			return "Protocol error.";
		case EPROTONOSUPPORT:	return "Protocol not supported.";
		case EPROTOTYPE:		return "Protocol wrong type for socket.";
		case ERANGE:			return "Result too large.";
		case EROFS:				return "Read-only file system.";
		case ESPIPE:			return "Invalid seek.";
		case ESRCH:				return "No such process.";
		case ESTALE:			return "Reserved.";
		case ETIME:				return "Stream ioctl() timeout.";
		case ETIMEDOUT:			return "Connection timed out.";
		case ETXTBSY:			return "Text file busy.";
		case EWOULDBLOCK:		return "Operation would block.";
		case EXDEV:				return "Cross-device link.";
		case ENOTBLK:			return "Block device required";
		case EUNKNOWN:			return "Unknown error";
	}

	errno = EINVAL;
	return nullptr;
}
