#include <BAN/Bitcast.h>
#include <kernel/Debug.h>
#include <kernel/InterruptStack.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Syscall.h>
#include <kernel/Timer/Timer.h>

#include <termios.h>

#define DUMP_ALL_SYSCALLS 0
#define DUMP_LONG_SYSCALLS 0

namespace Kernel
{

	extern "C" long sys_fork(uintptr_t sp, uintptr_t ip)
	{
		auto ret = Process::current().sys_fork(sp, ip);
		if (ret.is_error())
			return -ret.error().get_error_code();
		return ret.value();
	}

	extern "C" long sys_fork_trampoline();

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

	extern "C" long cpp_syscall_handler(int syscall, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, InterruptStack* interrupt_stack)
	{
		ASSERT(GDT::is_user_segment(interrupt_stack->cs));

		Processor::set_interrupt_state(InterruptState::Enabled);

		BAN::ErrorOr<long> ret = BAN::Error::from_errno(ENOSYS);

		const char* process_path = Process::current().name();

#if DUMP_ALL_SYSCALLS
		dprintln("{} pid {}: {}", process_path, Process::current().pid(), s_syscall_names[syscall]);
#endif

#if DUMP_LONG_SYSCALLS
		const uint64_t start_ns = SystemTimer::get().ns_since_boot();
#endif

		if (syscall < 0 || syscall >= __SYSCALL_COUNT)
			dwarnln("No syscall {}", syscall);
		else if (syscall == SYS_FORK)
			ret = sys_fork_trampoline();
		else
#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wmaybe-uninitialized"
			ret = (Process::current().*s_syscall_handlers[syscall])(arg1, arg2, arg3, arg4, arg5);
#pragma GCC diagnostic pop

#if DUMP_ALL_SYSCALLS
		if (ret.is_error())
			dprintln("{} pid {}: {}: {}", process_path, Process::current().pid(), s_syscall_names[syscall], ret.error());
		else
			dprintln("{} pid {}: {}: {}", process_path, Process::current().pid(), s_syscall_names[syscall], ret.value());
#else
		if (ret.is_error() && ret.error().get_error_code() == ENOTSUP)
			dwarnln("{} pid {}: {}: ENOTSUP", process_path, Process::current().pid(), s_syscall_names[syscall]);
#endif

#if DUMP_LONG_SYSCALLS
		const uint64_t end_ns = SystemTimer::get().ns_since_boot();
		const uint64_t duration_us = (end_ns - start_ns) / 1000;
		if (duration_us > 1'000)
			dwarnln("{} {} took {}.{3} ms",
				Process::current().name(),
				s_syscall_names[syscall],
				duration_us / 1000, duration_us % 1000
			);
#endif

		if (ret.is_error() && ret.error().is_kernel_error())
			Kernel::panic("Kernel error while returning to userspace {}", ret.error());

		Processor::set_interrupt_state(InterruptState::Disabled);

		auto& current_thread = Thread::current();
		if (current_thread.can_add_signal_to_execute())
			if (current_thread.handle_signal())
				if (ret.is_error() && ret.error().get_error_code() == EINTR && is_restartable_syscall(syscall))
					ret = BAN::Error::from_errno(ERESTART);

		ASSERT(Kernel::Thread::current().state() == Kernel::Thread::State::Executing);

		if (ret.is_error())
			return -ret.error().get_error_code();
		return ret.value();
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
			case SYS_RECVFROM:
			case SYS_SENDTO:
			case SYS_FLOCK:
				return true;
			default:
				return false;
		}
	}

}
