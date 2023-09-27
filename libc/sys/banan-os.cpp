#include <sys/banan-os.h>
#include <sys/syscall.h>
#include <unistd.h>

int tty_ctrl(int fildes, int command, int flags)
{
	return syscall(SYS_TTY_CTRL, fildes, command, flags);
}
