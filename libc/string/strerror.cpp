#include <errno.h>
#include <string.h>

int errno = 0;

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