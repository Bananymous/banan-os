#ifndef _SYS_BANAN_OS_H
#define _SYS_BANAN_OS_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#define __need_size_t 1
#include <stddef.h>

#define TTY_CMD_SET		0x01
#define TTY_CMD_UNSET	0x02

#define TTY_FLAG_ENABLE_OUTPUT	1
#define TTY_FLAG_ENABLE_INPUT	2

#define POWEROFF_SHUTDOWN 0
#define POWEROFF_REBOOT 1

struct proc_meminfo_t
{
	size_t page_size;
	size_t virt_pages;
	size_t phys_pages;
};

/*
fildes:		refers to valid tty device
command:	one of TTY_CMD_* definitions
flags:		bitwise or of TTY_FLAG_* definitions

return value: 0 on success, -1 on failure and errno set to the error
*/
int tty_ctrl(int fildes, int command, int flags);
int poweroff(int command);

int load_keymap(const char* path);

// Create shared memory object and return its key or -1 on error
long smo_create(size_t size, int prot);
// Delete shared memory object such that it will be no longer accessible with smo_map(). Existing mappings are still valid
int smo_delete(long key);
// Map shared memory object defined by its key and return address or null on error. Mappings can be unmapped using munmap()
void* smo_map(long key);

__END_DECLS

#endif
