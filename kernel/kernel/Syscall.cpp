#include <kernel/Debug.h>
#include <kernel/Process.h>
#include <kernel/Syscall.h>

namespace Kernel
{

	void sys_exit()
	{
		Process::current()->exit();
	}

	int sys_read(int fd, void* buffer, size_t size)
	{
		auto res = Process::current()->read(fd, buffer, size);
		if (res.is_error())
			return res.error().get_error_code();
		return 0;
	}

	int sys_write(int fd, const void* buffer, size_t size)
	{
		auto res = Process::current()->write(fd, buffer, size);
		if (res.is_error())
			return res.error().get_error_code();
		return 0;
	}

	extern "C" int cpp_syscall_handler(int syscall, void* arg1, void* arg2, void* arg3)
	{
		(void)arg1;
		(void)arg2;
		(void)arg3;

		int ret = 0;
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
		default:
			ret = -1;
			dprintln("Unknown syscall");
			break;
		}

		return ret;
	}

}