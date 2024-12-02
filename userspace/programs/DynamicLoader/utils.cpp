#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void print(int fd, const char* buffer)
{
	size_t len = 0;
	while (buffer[len])
		len++;
	syscall(SYS_WRITE, fd, buffer, len);
}

static const char* errno_to_string(int error);

[[noreturn]] void print_error_and_exit(const char* message, int error)
{
	print(STDERR_FILENO, message);
	if (error < 0)
	{
		print(STDERR_FILENO, ": ");
		print(STDERR_FILENO, errno_to_string(-error));
	}
	print(STDERR_FILENO, "\n");

	syscall(SYS_EXIT, 1);
	__builtin_unreachable();
}

int strcmp(const char* s1, const char* s2)
{
	const unsigned char* u1 = reinterpret_cast<const unsigned char*>(s1);
	const unsigned char* u2 = reinterpret_cast<const unsigned char*>(s2);
	for (; *u1 && *u2; u1++, u2++)
		if (*u1 != *u2)
			break;
	return *u1 - *u2;
}

char* strcpy(char* __restrict s1, const char* __restrict s2)
{
	size_t i = 0;
	for (; s2[i]; i++)
		s1[i] = s2[i];
	s1[i] = '\0';
	return s1;
}

void* memcpy(void* __restrict dstp, const void* __restrict srcp, size_t n)
{
	unsigned char* dst = static_cast<unsigned char*>(dstp);
	const unsigned char* src = static_cast<const unsigned char*>(srcp);
	for (size_t i = 0; i < n; i++)
		dst[i] = src[i];
	return dstp;
}

void* memset(void* s, int c, size_t n)
{
	unsigned char* p = static_cast<unsigned char*>(s);
	for (size_t i = 0; i < n; i++)
		p[i] = c;
	return s;
}

static int s_random_fd;

void init_random()
{
	s_random_fd = syscall(SYS_OPENAT, AT_FDCWD, "/dev/random", O_RDONLY);
	if (s_random_fd < 0)
		print_error_and_exit("could not open /dev/random", s_random_fd);
}

void fini_random()
{
	auto ret = syscall(SYS_CLOSE, s_random_fd);
	if (ret < 0)
		print_error_and_exit("could not close /dev/random", ret);
}

uintptr_t get_random_uptr()
{
	uintptr_t result;
	if (auto ret = syscall(SYS_READ, s_random_fd, &result, sizeof(result)); ret != sizeof(result))
		print_error_and_exit("could not read from /dev/random", ret);
	return result;
}

static const char* errno_to_string(int error)
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
	return nullptr;
}
