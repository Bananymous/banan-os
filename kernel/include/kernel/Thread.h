#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/RefPtr.h>
#include <BAN/UniqPtr.h>
#include <kernel/InterruptStack.h>
#include <kernel/Memory/VirtualRange.h>
#include <kernel/ThreadBlocker.h>

#include <signal.h>
#include <sys/types.h>

namespace Kernel
{

	class Process;

	class Thread
	{
		BAN_NON_COPYABLE(Thread);
		BAN_NON_MOVABLE(Thread);

	public:
		using entry_t = void(*)(void*);

		enum class State
		{
			NotStarted,
			Executing,
			Terminated,
		};

	public:
		static BAN::ErrorOr<Thread*> create_kernel(entry_t, void*, Process*);
		static BAN::ErrorOr<Thread*> create_userspace(Process*);
		~Thread();

		BAN::ErrorOr<Thread*> clone(Process*, uintptr_t sp, uintptr_t ip);
		void setup_exec();
		void setup_process_cleanup();

		// Returns true, if thread is going to trigger signal
		bool is_interrupted_by_signal() const;

		// Returns true if pending signal can be added to thread
		bool can_add_signal_to_execute() const;
		bool will_execute_signal() const;
		void handle_signal(int signal = 0);
		bool add_signal(int signal);

		// blocks current thread and returns either on unblock, eintr, spuriously or after timeout
		BAN::ErrorOr<void> sleep_or_eintr_ms(uint64_t ms) { return sleep_or_eintr_ns(ms * 1'000'000); }
		BAN::ErrorOr<void> sleep_or_eintr_ns(uint64_t ns);
		BAN::ErrorOr<void> block_or_eintr_indefinite(ThreadBlocker& thread_blocker);
		BAN::ErrorOr<void> block_or_eintr_or_timeout_ms(ThreadBlocker& thread_blocker, uint64_t timeout_ms, bool etimedout) { return block_or_eintr_or_timeout_ns(thread_blocker, timeout_ms * 1'000'000, etimedout); }
		BAN::ErrorOr<void> block_or_eintr_or_waketime_ms(ThreadBlocker& thread_blocker, uint64_t wake_time_ms, bool etimedout) { return block_or_eintr_or_waketime_ns(thread_blocker, wake_time_ms * 1'000'000, etimedout); }
		BAN::ErrorOr<void> block_or_eintr_or_timeout_ns(ThreadBlocker& thread_blocker, uint64_t timeout_ns, bool etimedout);
		BAN::ErrorOr<void> block_or_eintr_or_waketime_ns(ThreadBlocker& thread_blocker, uint64_t wake_time_ns, bool etimedout);

		pid_t tid() const { return m_tid; }

		State state() const { return m_state; }

		vaddr_t kernel_stack_bottom() const	{ return m_kernel_stack->vaddr(); }
		vaddr_t kernel_stack_top() const	{ return m_kernel_stack->vaddr() + m_kernel_stack->size(); }
		VirtualRange& kernel_stack() { return *m_kernel_stack; }

		vaddr_t userspace_stack_bottom() const	{ return is_userspace() ? m_userspace_stack->vaddr() : UINTPTR_MAX; }
		vaddr_t userspace_stack_top() const		{ return is_userspace() ? m_userspace_stack->vaddr() + m_userspace_stack->size() : 0; }
		VirtualRange& userspace_stack() { ASSERT(is_userspace()); return *m_userspace_stack; }

		static Thread& current();
		static pid_t current_tid();

		Process& process();
		const Process& process() const;
		bool has_process() const { return m_process; }

		bool is_userspace() const { return m_is_userspace; }

		size_t virtual_page_count() const { return (m_kernel_stack ? (m_kernel_stack->size() / PAGE_SIZE) : 0) + (m_userspace_stack ? (m_userspace_stack->size() / PAGE_SIZE) : 0); }
		size_t physical_page_count() const { return virtual_page_count(); }

		InterruptStack& interrupt_stack() { return m_interrupt_stack; }
		InterruptRegisters& interrupt_registers() { return m_interrupt_registers; }

#if __enable_sse
		void save_sse();
		void load_sse();
#endif

		void add_mutex() { m_mutex_count++; }
		void remove_mutex() { m_mutex_count--; }

	private:
		Thread(pid_t tid, Process*);

		static void on_exit_trampoline(Thread*);
		void on_exit();

	private:
		static constexpr size_t    m_kernel_stack_size    { PAGE_SIZE * 64 };
		static constexpr size_t    m_userspace_stack_size { PAGE_SIZE * 64 };
		BAN::UniqPtr<VirtualRange> m_kernel_stack;
		BAN::UniqPtr<VirtualRange> m_userspace_stack;
		const pid_t                m_tid                  { 0 };
		State                      m_state                { State::NotStarted };
		Process*                   m_process              { nullptr };
		bool                       m_is_userspace         { false };
		bool                       m_delete_process       { false };

		SchedulerQueue::Node*      m_scheduler_node       { nullptr };

		InterruptStack             m_interrupt_stack      { };
		InterruptRegisters         m_interrupt_registers  { };

		uint64_t                   m_signal_pending_mask  { 0 };
		uint64_t                   m_signal_block_mask    { 0 };
		SpinLock                   m_signal_lock;
		static_assert(_SIGMAX < 64);

		BAN::Atomic<uint32_t>      m_mutex_count          { 0 };

#if __enable_sse
		alignas(16) uint8_t m_sse_storage[512] {};
#endif

		friend class Process;
		friend class Scheduler;
	};

}
