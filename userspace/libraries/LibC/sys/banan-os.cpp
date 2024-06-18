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

long smo_create(size_t size, int prot)
{
	return syscall(SYS_SMO_CREATE, size, prot);
}

int smo_delete(long key)
{
	return syscall(SYS_SMO_DELETE, key);
}

void* smo_map(long key)
{
	long ret = syscall(SYS_SMO_MAP, key);
	if (ret < 0)
		return nullptr;
	return reinterpret_cast<void*>(ret);
}
