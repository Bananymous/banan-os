#include <pthread.h>
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

int mprotect(void* addr, size_t len, int prot)
{
	return syscall(SYS_MPROTECT, addr, len, prot);
}

int msync(void* addr, size_t len, int flags)
{
	pthread_testcancel();
	return syscall(SYS_MSYNC, addr, len, flags);
}

int posix_madvise(void* addr, size_t len, int advice)
{
	(void)addr;
	(void)len;
	(void)advice;
	fprintf(stddbg, "TODO: posix_madvise");
	return 0;
}

#include <BAN/Assert.h>
#include <BAN/Debug.h>
#include <errno.h>

int mlock(const void*, size_t)
{
	ASSERT_NOT_REACHED();
}

int shm_open(const char* name, int oflag, mode_t mode)
{
	(void)name;
	(void)oflag;
	(void)mode;
	dwarnln("TODO: shm_open");
	errno = ENOTSUP;
	return -1;
}

int shm_unlink(const char* name)
{
	(void)name;
	dwarnln("TODO: shm_unlink");
	errno = ENOTSUP;
	return -1;
}
