#include <kernel/Debug.h>
#include <kernel/Process.h>
#include <kernel/Syscall.h>

namespace Kernel
{

	void sys_exit()
	{
		Process::current().exit();
	}

	long sys_read(int fd, void* buffer, size_t size)
	{
		auto res = Process::current().read(fd, buffer, size);
		if (res.is_error())
			return -res.error().get_error_code();
		return res.value();
	}

	long sys_write(int fd, const void* buffer, size_t size)
	{
		auto res = Process::current().write(fd, buffer, size);
		if (res.is_error())
			return -res.error().get_error_code();
		return res.value();
	}

	int sys_close(int fd)
	{
		auto res = Process::current().close(fd);
		if (res.is_error())
			return -res.error().get_error_code();
		return 0;
	}

	int sys_seek(int fd, long offset)
	{
		auto res = Process::current().seek(fd, offset);
		if (res.is_error())
			return -res.error().get_error_code();
		return 0;
	}

	void sys_termid(char* buffer)
	{
		Process::current().termid(buffer);
	}

	int sys_open(const char* path, int oflags)
	{
		auto res = Process::current().open(path, oflags);
		if (res.is_error())
			return -res.error().get_error_code();
		return res.value();
	}

	extern "C" long cpp_syscall_handler(int syscall, void* arg1, void* arg2, void* arg3)
	{
		Thread::current().set_in_syscall(true);

		asm volatile("sti");

		long ret = 0;
		switch (syscall)
		{
		case SYS_EXIT:
			sys_exit();
			break;
		case SYS_READ:
			ret = sys_read((int)(uintptr_t)arg1, arg2, (size_t)(uintptr_t)arg3);
			break;
		case SYS_WRITE:
			ret = sys_write((int)(uintptr_t)arg1, arg2, (size_t)(uintptr_t)arg3);
			break;
		case SYS_TERMID:
			sys_termid((char*)arg1);
			break;
		case SYS_CLOSE:
			ret = sys_close((int)(uintptr_t)arg1);
			break;
		case SYS_SEEK:
			ret = sys_seek((int)(uintptr_t)arg1, (long)arg2);
			break;
		case SYS_OPEN:
			ret = sys_open((const char*)arg1, (int)(uintptr_t)arg2);
			break;
		default:
			Kernel::panic("Unknown syscall {}", syscall);
		}

		asm volatile("cli");

		Thread::current().set_in_syscall(false);

		return ret;
	}

}