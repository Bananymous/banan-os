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

char* stpcpy(char* __restrict__ dest, const char* __restrict__ src)
{
	size_t i = 0;
	for (; src[i]; i++)
		dest[i] = src[i];
	dest[i] = '\0';
	return &dest[i];
}

char* stpncpy(char* __restrict__ dest, const char* __restrict__ src, size_t n)
{
	size_t i = 0;
	for (; src[i] && n; i++, n--)
		dest[i] = src[i];
	for (; n; i++, n--)
		dest[i] = '\0';
	dest[i] = '\0';
	return &dest[i];
}

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
	strncpy(dest + strlen(dest), src, n);
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
		case EEXISTS:			return "EEXISTS";
		case ENOTBLK:			return "ENOTBLK";
		case EUNKNOWN:			return "EUNKNOWN";
	}

	errno = EINVAL;
	return "EUNKNOWN";
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
		case EEXISTS:			return "File exists";
		case ENOTBLK:			return "Block device required";
		case EUNKNOWN:			return "Unknown error";
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

char* strchrnul(const char* str, int c)
{
	while (*str)
	{
		if (*str == c)
			return (char*)str;
		str++;
	}
	return (char*)str;
}

char* strstr(const char* haystack, const char* needle)
{
	for (size_t i = 0; haystack[i]; i++)
		if (memcmp(haystack + i, needle, strlen(needle)) == 0)
			return (char*)haystack + i;
	return NULL;
}
