#include <errno.h>
#include <string.h>

int errno = 0;

char* strerror(int error)
{
	switch (error)
	{
	case ENOMEM:
		return "Cannot allocate memory";
	case EINVAL:
		return "Invalid argument";
	case EISDIR:
		return "Is a directory";
	case ENOTDIR:
		return "Not a directory";
	case ENOENT:
		return "No such file or directory";
	case EIO:
		return "Input/output error";
	default:
		break;
	}

	// FIXME: sprintf
	//static char buffer[26];
	//sprintf(buffer, "Unknown error %d", error);
	//return buffer;
	errno = EINVAL;
	return "Unknown error";
}