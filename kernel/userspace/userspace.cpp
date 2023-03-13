#include <BAN/Formatter.h>
#include <kernel/Syscall.h>

#define USERSPACE __attribute__((section(".userspace")))

USERSPACE int syscall(int syscall, void* arg1 = nullptr, void* arg2 = nullptr, void* arg3 = nullptr)
{
	int ret;
	asm volatile("int $0x80" : "=a"(ret) : "a"(syscall), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
	return ret;
}

USERSPACE void user_putc(char ch)
{
	syscall(SYS_PUTC, (void*)(uintptr_t)ch);
}

USERSPACE void print(const char* str)
{
	while (*str)
		user_putc(*str++);
}

USERSPACE void userspace_entry()
{
	BAN::Formatter::println(user_putc, "Hello {}!", "World");
	for (;;);
}
