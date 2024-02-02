#include <stdarg.h>
#include <stropts.h>
#include <sys/syscall.h>
#include <unistd.h>

int ioctl(int fildes, int request, ...)
{
	va_list args;
	va_start(args, request);
	void* extra = va_arg(args, void*);
	va_end(args);

	return syscall(SYS_IOCTL, fildes, request, extra);
}
