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

int mlock(const void*, size_t)
{
	ASSERT_NOT_REACHED();
}

int mprotect(void*, size_t, int)
{
	ASSERT_NOT_REACHED();
}
