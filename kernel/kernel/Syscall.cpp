#include <kernel/Debug.h>
#include <kernel/Process.h>
#include <kernel/Syscall.h>

#include <termios.h>

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

	long sys_alloc(size_t bytes)
	{
		auto res = Process::current().allocate(bytes);
		if (res.is_error())
			return -res.error().get_error_code();
		return (long)res.value();
	}

	void sys_free(void* ptr)
	{
		Process::current().free(ptr);
	}

	long sys_seek(int fd, long offset, int whence)
	{
		auto res = Process::current().seek(fd, offset, whence);
		if (res.is_error())
			return -res.error().get_error_code();
		return 0;
	}

	long sys_tell(int fd)
	{
		auto res = Process::current().tell(fd);
		if (res.is_error())
			return -res.error().get_error_code();
		return res.value();
	}

	long sys_get_termios(::termios* termios)
	{
		auto current = Process::current().tty().get_termios();
		memset(termios, 0, sizeof(::termios));
		if (current.canonical)
			termios->c_lflag |= ICANON;
		if (current.echo)
			termios->c_lflag |= ECHO;
		return 0;
	}

	long sys_set_termios(const ::termios* termios)
	{
		Kernel::termios new_termios;
		new_termios.canonical = termios->c_lflag & ICANON;
		new_termios.echo = termios->c_lflag & ECHO;
		Process::current().tty().set_termios(new_termios);
		return 0;
	}

	extern "C" long sys_fork(uintptr_t rsp, uintptr_t rip)
	{
		auto ret = Process::current().fork(rsp, rip);
		if (ret.is_error())
			return -ret.error().get_error_code();
		return ret.value()->pid();
	}

	long sys_sleep(unsigned int seconds)
	{
		PIT::sleep(seconds * 1000);
		return 0;
	}

	long sys_exec(const char* pathname, const char* const* argv, const char* const* envp)
	{
		auto ret = Process::current().exec(pathname, argv, envp);
		if (ret.is_error())
			return -ret.error().get_error_code();
		ASSERT_NOT_REACHED();
	}

	long sys_wait(pid_t pid, int* stat_loc, int options)
	{
		auto ret = Process::current().wait(pid, stat_loc, options);
		if (ret.is_error())
			return -ret.error().get_error_code();
		return ret.value();
	}

	long sys_stat(const char* path, struct stat* buf, int flags)
	{
		auto ret = Process::current().stat(path, buf, flags);
		if (ret.is_error())
			return -ret.error().get_error_code();
		return 0;
	}

	long sys_setenvp(char** envp)
	{
		auto ret = Process::current().setenvp(envp);
		if (ret.is_error())
			return -ret.error().get_error_code();
		return 0;
	}

	extern "C" long sys_fork_trampoline();

	extern "C" long cpp_syscall_handler(int syscall, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
	{
		Thread::current().set_in_syscall(true);

		asm volatile("sti");

		(void)arg1;
		(void)arg2;
		(void)arg3;
		(void)arg4;
		(void)arg5;

		long ret = 0;
		switch (syscall)
		{
		case SYS_EXIT:
			sys_exit();
			break;
		case SYS_READ:
			ret = sys_read((int)arg1, (void*)arg2, (size_t)arg3);
			break;
		case SYS_WRITE:
			ret = sys_write((int)arg1, (const void*)arg2, (size_t)arg3);
			break;
		case SYS_TERMID:
			sys_termid((char*)arg1);
			break;
		case SYS_CLOSE:
			ret = sys_close((int)arg1);
			break;
		case SYS_OPEN:
			ret = sys_open((const char*)arg1, (int)arg2);
			break;
		case SYS_ALLOC:
			ret = sys_alloc((size_t)arg1);
			break;
		case SYS_FREE:
			sys_free((void*)arg1);
			break;
		case SYS_SEEK:
			ret = sys_seek((int)arg1, (long)arg2, (int)arg3);
			break;
		case SYS_TELL:
			ret = sys_tell((int)arg1);
			break;
		case SYS_GET_TERMIOS:
			ret = sys_get_termios((::termios*)arg1);
			break;
		case SYS_SET_TERMIOS:
			ret = sys_set_termios((const ::termios*)arg1);
			break;
		case SYS_FORK:
			ret = sys_fork_trampoline();
			break;
		case SYS_SLEEP:
			ret = sys_sleep((unsigned int)arg1);
			break;
		case SYS_EXEC:
			ret = sys_exec((const char*)arg1, (const char* const*)arg2, (const char* const*)arg3);
			break;
		case SYS_WAIT:
			ret = sys_wait((pid_t)arg1, (int*)arg2, (int)arg3);
			break;
		case SYS_STAT:
			ret = sys_stat((const char*)arg1, (struct stat*)arg2, (int)arg3);
			break;
		case SYS_SETENVP:
			ret = sys_setenvp((char**)arg1);
			break;
		default:
			Kernel::panic("Unknown syscall {}", syscall);
		}

		asm volatile("cli");

		Thread::current().set_in_syscall(false);

		return ret;
	}

}