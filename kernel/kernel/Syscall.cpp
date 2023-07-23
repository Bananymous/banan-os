#include <kernel/Debug.h>
#include <kernel/InterruptStack.h>
#include <kernel/Process.h>
#include <kernel/Syscall.h>

#include <termios.h>

namespace Kernel
{

	extern "C" long sys_fork(uintptr_t rsp, uintptr_t rip)
	{
		auto ret = Process::current().sys_fork(rsp, rip);
		if (ret.is_error())
			return -ret.error().get_error_code();
		return ret.value();
	}

	extern "C" long sys_fork_trampoline();

	extern "C" long cpp_syscall_handler(int syscall, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, InterruptStack& interrupt_stack)
	{
		Thread::current().set_in_syscall(true);

		if (syscall == SYS_SIGNAL_DONE)
		{
			Thread::current().set_signal_done((int)arg1);
			return 0;
		}

		asm volatile("sti");

		(void)arg1;
		(void)arg2;
		(void)arg3;
		(void)arg4;
		(void)arg5;
		(void)interrupt_stack;

		BAN::ErrorOr<long> ret = BAN::Error::from_errno(ENOSYS);

		switch (syscall)
		{
		case SYS_EXIT:
			ret = Process::current().sys_exit((int)arg1);
			break;
		case SYS_READ:
			ret = Process::current().sys_read((int)arg1, (void*)arg2, (size_t)arg3);
			break;
		case SYS_WRITE:
			ret = Process::current().sys_write((int)arg1, (const void*)arg2, (size_t)arg3);
			break;
		case SYS_TERMID:
			ret = Process::current().sys_termid((char*)arg1);
			break;
		case SYS_CLOSE:
			ret = Process::current().sys_close((int)arg1);
			break;
		case SYS_OPEN:
			ret = Process::current().sys_open((const char*)arg1, (int)arg2, (mode_t)arg3);
			break;
		case SYS_OPENAT:
			ret = Process::current().sys_openat((int)arg1, (const char*)arg2, (int)arg3, (mode_t)arg4);
			break;
		case SYS_ALLOC:
			ret = Process::current().sys_alloc((size_t)arg1);
			break;
		case SYS_FREE:
			ret = Process::current().sys_free((void*)arg1);
			break;
		case SYS_SEEK:
			ret = Process::current().sys_seek((int)arg1, (long)arg2, (int)arg3);
			break;
		case SYS_TELL:
			ret = Process::current().sys_tell((int)arg1);
			break;
		case SYS_GET_TERMIOS:
			ret = Process::current().sys_gettermios((::termios*)arg1);
			break;
		case SYS_SET_TERMIOS:
			ret = Process::current().sys_settermios((const ::termios*)arg1);
			break;
		case SYS_FORK:
			ret = sys_fork_trampoline();
			break;
		case SYS_EXEC:
			ret = Process::current().sys_exec((const char*)arg1, (const char* const*)arg2, (const char* const*)arg3);
			break;
		case SYS_SLEEP:
			ret = Process::current().sys_sleep((unsigned int)arg1);
			break;
		case SYS_WAIT:
			ret = Process::current().sys_wait((pid_t)arg1, (int*)arg2, (int)arg3);
			break;
		case SYS_FSTAT:
			ret = Process::current().sys_fstat((int)arg1, (struct stat*)arg2);
			break;
		case SYS_SETENVP:
			ret = Process::current().sys_setenvp((char**)arg1);
			break;
		case SYS_READ_DIR_ENTRIES:
			ret = Process::current().sys_read_dir_entries((int)arg1, (API::DirectoryEntryList*)arg2, (size_t)arg3);
			break;
		case SYS_SET_UID:
			ret = Process::current().sys_setuid((uid_t)arg1);
			break;
		case SYS_SET_GID:
			ret = Process::current().sys_setgid((gid_t)arg1);
			break;
		case SYS_SET_EUID:
			ret = Process::current().sys_seteuid((uid_t)arg1);
			break;
		case SYS_SET_EGID:
			ret = Process::current().sys_setegid((gid_t)arg1);
			break;
		case SYS_SET_REUID:
			ret = Process::current().sys_setreuid((uid_t)arg1, (uid_t)arg2);
			break;
		case SYS_SET_REGID:
			ret = Process::current().sys_setregid((gid_t)arg1, (gid_t)arg2);
			break;
		case SYS_GET_UID:
			ret = Process::current().sys_getuid();
			break;
		case SYS_GET_GID:
			ret = Process::current().sys_getgid();
			break;
		case SYS_GET_EUID:
			ret = Process::current().sys_geteuid();
			break;
		case SYS_GET_EGID:
			ret = Process::current().sys_getegid();
			break;
		case SYS_GET_PWD:
			ret = Process::current().sys_getpwd((char*)arg1, (size_t)arg2);
			break;
		case SYS_SET_PWD:
			ret = Process::current().sys_setpwd((const char*)arg1);
			break;
		case SYS_CLOCK_GETTIME:
			ret = Process::current().sys_clock_gettime((clockid_t)arg1, (timespec*)arg2);
			break;
		case SYS_PIPE:
			ret = Process::current().sys_pipe((int*)arg1);
			break;
		case SYS_DUP2:
			ret = Process::current().sys_dup2((int)arg1, (int)arg2);
			break;
		case SYS_RAISE:
			ret = Process::current().sys_raise((int)arg1, interrupt_stack.rsp, interrupt_stack.rip);
			break;
		case SYS_KILL:
			ret = Process::current().sys_kill((pid_t)arg1, (int)arg2, interrupt_stack.rsp, interrupt_stack.rip);
			break;
		case SYS_SIGNAL:
			ret = Process::current().sys_signal((int)arg1, (void (*)(int))arg2);
			break;
		case SYS_SIGNAL_DONE:
			// Handled above
			ASSERT_NOT_REACHED();
		default:
			dwarnln("Unknown syscall {}", syscall);
			break;
		}

		asm volatile("cli");

		Thread::current().set_in_syscall(false);

		if (ret.is_error())
			return -ret.error().get_error_code();
		return ret.value();
	}

}