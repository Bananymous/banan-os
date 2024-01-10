#include <sys/banan-os.h>
#include <sys/syscall.h>
#include <unistd.h>

int tty_ctrl(int fildes, int command, int flags)
{
	return syscall(SYS_TTY_CTRL, fildes, command, flags);
}

int poweroff(int command)
{
	return syscall(SYS_POWEROFF, command);
}

int load_keymap(const char* path)
{
	return syscall(SYS_LOAD_KEYMAP, path);
}
