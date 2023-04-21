#include <BAN/Formatter.h>
#include <kernel/Syscall.h>

#include <unistd.h>

#define USERSPACE __attribute__((section(".userspace")))

USERSPACE void userspace_entry()
{
	Kernel::syscall(SYS_WRITE, STDOUT_FILENO, "Hello World!", 12);

	char buffer[128];
	while (true)
	{
		long n_read = Kernel::syscall(SYS_READ, STDIN_FILENO, buffer, sizeof(buffer));
		Kernel::syscall(SYS_WRITE, STDOUT_FILENO, buffer, n_read);
	}

	Kernel::syscall(SYS_EXIT);
}
