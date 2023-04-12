#include <BAN/Formatter.h>
#include <kernel/Syscall.h>

#include <unistd.h>

#define USERSPACE __attribute__((section(".userspace")))

USERSPACE void userspace_entry()
{
	Kernel::syscall(SYS_WRITE, STDOUT_FILENO, "Hello World!", 12);
	Kernel::syscall(SYS_EXIT);
}
