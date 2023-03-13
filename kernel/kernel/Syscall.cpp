#include <kernel/Debug.h>
#include <kernel/kprint.h>
#include <kernel/Syscall.h>

namespace Kernel
{

	int sys_test()
	{
		dprintln("hello");
		return 0;
	}

	int sys_putc(void* ch)
	{
		kprint("{}", (char)(uintptr_t)ch);
		return 0;
	}

	extern "C" int cpp_syscall_handler(int syscall, void* arg1, void* arg2, void* arg3)
	{
		(void)arg1;
		(void)arg2;
		(void)arg3;

		int ret;
		switch (syscall)
		{
		case SYS_TEST:
			ret = sys_test();
			break;
		case SYS_PUTC:
			ret = sys_putc(arg1);
			break;
		default:
			ret = -1;
			dprintln("Unknown syscall");
			break;
		}

		return ret;
	}

}