#include <BAN/Bitcast.h>
#include <kernel/API/Syscall.h>
#include <kernel/Debug.h>
#include <kernel/InterruptStack.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Timer/Timer.h>

#include <termios.h>
#include <sys/syscall.h>

#define DUMP_ALL_SYSCALLS 0
#define DUMP_UNSUPPOTED_SYSCALLS 1

#define SYSCALL_DUMP_SKIP_COND false

namespace Kernel
{

	using SyscallHandler = BAN::ErrorOr<long> (Process::*)(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t);

	static const SyscallHandler s_syscall_handlers[] = {
#define O(enum, name) BAN::bit_cast<SyscallHandler>(&Process::sys_ ## name),
		__SYSCALL_LIST(O)
#undef O
	};

	static constexpr const char* s_syscall_names[] {
#define O(enum, name) #enum,
		__SYSCALL_LIST(O)
#undef O
	};

	static bool is_restartable_syscall(int syscall);

	extern "C" long cpp_syscall_handler(int syscall, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
	{
		auto& process = Process::current();
		auto& thread = Thread::current();

		if (syscall < 0 || syscall >= __SYSCALL_COUNT)
		{
			dwarnln("{} {}:{}: Invalid syscall {}",
				process.name(),
				process.pid(),
				thread.tid(),
				syscall
			);
			return -ENOSYS;
		}

		thread.set_is_in_syscall(true);
		Processor::set_interrupt_state(InterruptState::Enabled);
		process.wait_while_stopped();

#if DUMP_ALL_SYSCALLS
		if (!(SYSCALL_DUMP_SKIP_COND))
			dprintln("{} {}:{}: {}", process.name(), process.pid(), thread.tid(), s_syscall_names[syscall]);
#endif

		auto result = (Process::current().*s_syscall_handlers[syscall])(arg1, arg2, arg3, arg4, arg5);

#if DUMP_ALL_SYSCALLS
		if (SYSCALL_DUMP_SKIP_COND)
			;
		else if (result.is_error())
			dprintln("{} {}:{}: {} -> {}", process.name(), process.pid(), thread.tid(), s_syscall_names[syscall], result.error());
		else
			dprintln("{} {}:{}: {} -> {}", process.name(), process.pid(), thread.tid(), s_syscall_names[syscall], result.value());
#elif DUMP_UNSUPPOTED_SYSCALLS
		if (result.is_error() && result.error().get_error_code() == ENOTSUP)
			dprintln("{} {}:{}: {} -> {}", process.name(), process.pid(), thread.tid(), s_syscall_names[syscall], result.error());
#endif

		Process::current().wait_while_stopped();
		if (Thread::current().handle_signal_if_interrupted())
			if (result.is_error() && result.error().get_error_code() == EINTR && is_restartable_syscall(syscall))
				result = BAN::Error::from_errno(ERESTART);
		Processor::set_interrupt_state(InterruptState::Disabled);
		Thread::current().set_is_in_syscall(false);

		ASSERT(Kernel::Thread::current().state() == Kernel::Thread::State::Executing);

		if (result.is_error())
			return -result.error().get_error_code();
		return result.value();
	}

	bool is_restartable_syscall(int syscall)
	{
		// https://www.man7.org/linux/man-pages/man7/signal.7.html
		// Interruption of system calls and library functions by signal handlers
		switch (syscall)
		{
			case SYS_READ:
			case SYS_WRITE:
			case SYS_IOCTL:
			case SYS_OPENAT:
			case SYS_WAIT:
			case SYS_ACCEPT:
			case SYS_CONNECT:
			case SYS_RECVMSG:
			case SYS_SENDMSG:
			case SYS_FLOCK:
			case SYS_FUTEX:
				return true;
			default:
				return false;
		}
	}

}
