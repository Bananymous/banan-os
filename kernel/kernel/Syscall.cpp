#include <BAN/Bitcast.h>
#include <kernel/Debug.h>
#include <kernel/InterruptStack.h>
#include <kernel/Process.h>
#include <kernel/Scheduler.h>
#include <kernel/Syscall.h>

#include <termios.h>

#define DUMP_ALL_SYSCALLS 0

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

	extern "C" long cpp_syscall_handler(int syscall, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, InterruptStack* interrupt_stack)
	{
		ASSERT(GDT::is_user_segment(interrupt_stack->cs));

		asm volatile("sti");

		BAN::ErrorOr<long> ret = BAN::Error::from_errno(ENOSYS);

		const char* process_path = nullptr;
		if (Process::current().userspace_info().argc > 0 && Process::current().userspace_info().argv)
			process_path = Process::current().userspace_info().argv[0];
		if (process_path == nullptr)
			process_path = "<null>";

#if DUMP_ALL_SYSCALLS
		dprintln("{} pid {}: {}", process_path, Process::current().pid(), s_syscall_names[syscall]);
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

		asm volatile("cli");

#if DUMP_ALL_SYSCALLS
		if (ret.is_error())
			dprintln("{} pid {}: {}: {}", process_path, Process::current().pid(), s_syscall_names[syscall], ret.error());
		else
			dprintln("{} pid {}: {}: {}", process_path, Process::current().pid(), s_syscall_names[syscall], ret.value());
#else
		if (ret.is_error() && ret.error().get_error_code() == ENOTSUP)
			dwarnln("{} pid {}: {}: ENOTSUP", process_path, Process::current().pid(), s_syscall_names[syscall]);
#endif

		if (ret.is_error() && ret.error().is_kernel_error())
			Kernel::panic("Kernel error while returning to userspace {}", ret.error());

		auto& current_thread = Thread::current();
		if (current_thread.can_add_signal_to_execute())
			current_thread.handle_signal();

		ASSERT(Kernel::Thread::current().state() == Kernel::Thread::State::Executing);

		if (ret.is_error())
			return -ret.error().get_error_code();
		return ret.value();
	}

}
