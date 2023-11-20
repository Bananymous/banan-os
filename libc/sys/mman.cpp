#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

void* mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off)
{
	sys_mmap_t args {
		.addr = addr,
		.len = len,
		.prot = prot,
		.flags = flags,
		.fildes = fildes,
		.off = off
	};
	long ret = syscall(SYS_MMAP, &args);
	if (ret == -1)
		return MAP_FAILED;
	return (void*)ret;
}

int munmap(void* addr, size_t len)
{
	return syscall(SYS_MUNMAP, addr, len);
}
