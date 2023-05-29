#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H 1

// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/sys_mman.h.html

#include <sys/cdefs.h>

__BEGIN_DECLS

#define PROT_EXEC	0x01
#define PROT_NONE	0x02
#define PROT_READ	0x04
#define PROT_WRITE	0x08

#define MAP_FIXED		0x01
#define MAP_PRIVATE		0x02
#define MAP_SHARED		0x04
#define MAP_ANONYMOUS	0x08

#define MS_ASYNC		0x01
#define MS_INVALIDATE	0x02
#define MS_SYNC			0x04

#define MCL_CURRENT	0x01
#define MCL_FUTURE	0x01

#define MAP_FAILED ((void*)0)

#define POSIX_MADV_DONTNEED		1
#define POSIX_MADV_NORMAL		2
#define POSIX_MADV_RANDOM		3
#define POSIX_MADV_SEQUENTIAL	4
#define POSIX_MADV_WILLNEED		5

#define POSIX_TYPED_MEM_ALLOCATE		0x01
#define POSIX_TYPED_MEM_ALLOCATE_CONTIG	0x02
#define POSIX_TYPED_MEM_MAP_ALLOCATABLE	0x04

#define __need_mode_t
#define __need_off_t
#define __need_size_t
#include <sys/types.h>

struct posix_typed_mem_info
{
	size_t posix_tmi_length;	/* Maximum length which may be allocated from a typed memory object. */
};

int		mlock(const void* addr, size_t len);
int		mlockall(int flags);
void*	mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off);
int		mprotect(void* addr, size_t len, int prot);
int		msync(void* addr, size_t len, int flags);
int		munlock(const void* addr, size_t len);
int		munlockall(void);
int		munmap(void* addr, size_t len);
int		posix_madvise(void* addr, size_t len, int advice);
int		posix_mem_offset(const void* __restrict addr, size_t len, off_t* __restrict off, size_t* __restrict contig_len, int* __restrict fildes);
int		posix_typed_mem_get_info(int fildes, struct posix_typed_mem_info* info);
int		posix_typed_mem_open(const char* name, int oflag, int tflag);
int		shm_open(const char* name, int oflag, mode_t mode);
int		shm_unlink(const char* name);

__END_DECLS

#endif
