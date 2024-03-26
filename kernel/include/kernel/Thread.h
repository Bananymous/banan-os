#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/RefPtr.h>
#include <BAN/UniqPtr.h>
#include <kernel/Memory/VirtualRange.h>

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
			Terminated
		};

	public:
		static BAN::ErrorOr<Thread*> create_kernel(entry_t, void*, Process*);
		static BAN::ErrorOr<Thread*> create_userspace(Process*);
		~Thread();

		BAN::ErrorOr<Thread*> clone(Process*, uintptr_t sp, uintptr_t ip);
		void setup_exec();
		void setup_process_cleanup();

		// Adds pending signals to thread if possible and
		// returns true, if thread is going to trigger signal
		bool is_interrupted_by_signal();

		// Returns true if pending signal can be added to thread
		bool can_add_signal_to_execute() const;
		bool will_execute_signal() const;
		void handle_signal(int signal = 0);
		bool add_signal(int signal);

		// blocks semaphore and returns either on unblock, eintr, spuriously or after timeout
		BAN::ErrorOr<void> block_or_eintr_indefinite(Semaphore& semaphore);
		BAN::ErrorOr<void> block_or_eintr_or_timeout(Semaphore& semaphore, uint64_t timeout_ms, bool etimedout);
		BAN::ErrorOr<void> block_or_eintr_or_waketime(Semaphore& semaphore, uint64_t wake_time_ms, bool etimedout);

		void set_return_sp(uintptr_t& sp) { m_return_sp = &sp; }
		void set_return_ip(uintptr_t& ip) { m_return_ip = &ip; }
		uintptr_t return_sp() { ASSERT(m_return_sp); return *m_return_sp; }
		uintptr_t return_ip() { ASSERT(m_return_ip); return *m_return_ip; }

		pid_t tid() const { return m_tid; }

		void set_sp(uintptr_t sp) { m_sp = sp; validate_stack(); }
		void set_ip(uintptr_t ip) { m_ip = ip; }
		uintptr_t sp() const { return m_sp; }
		uintptr_t ip() const { return m_ip; }

		void set_started() { ASSERT(m_state == State::NotStarted); m_state = State::Executing; }
		State state() const { return m_state; }

		vaddr_t stack_base() const { return m_stack->vaddr(); }
		size_t stack_size() const { return m_stack->size(); }
		VirtualRange& stack() { return *m_stack; }
		VirtualRange& interrupt_stack() { return *m_interrupt_stack; }

		vaddr_t interrupt_stack_base() const { return m_interrupt_stack ? m_interrupt_stack->vaddr() : 0; }
		size_t interrupt_stack_size() const { return m_interrupt_stack ? m_interrupt_stack->size() : 0; }

		static Thread& current();
		static pid_t current_tid();

		Process& process();
		const Process& process() const;
		bool has_process() const { return m_process; }

		bool is_userspace() const { return m_is_userspace; }

		size_t virtual_page_count() const { return m_stack->size() / PAGE_SIZE; }
		size_t physical_page_count() const { return virtual_page_count(); }

#if __enable_sse
		void save_sse();
		void load_sse();
		static Thread* sse_thread();
#endif

	private:
		Thread(pid_t tid, Process*);
		void on_exit();

		void validate_stack() const;

	private:
		static constexpr size_t		m_kernel_stack_size		= PAGE_SIZE * 4;
		static constexpr size_t		m_userspace_stack_size	= PAGE_SIZE * 2;
		static constexpr size_t		m_interrupt_stack_size	= PAGE_SIZE * 2;
		BAN::UniqPtr<VirtualRange>	m_interrupt_stack;
		BAN::UniqPtr<VirtualRange>	m_stack;
		uintptr_t					m_ip				{ 0 };
		uintptr_t					m_sp				{ 0 };
		const pid_t					m_tid				{ 0 };
		State						m_state				{ State::NotStarted };
		Process*					m_process			{ nullptr };
		bool						m_is_userspace		{ false };

		uintptr_t*					m_return_sp			{ nullptr };
		uintptr_t*					m_return_ip			{ nullptr };

		uint64_t					m_signal_pending_mask	{ 0 };
		uint64_t					m_signal_block_mask		{ 0 };
		SpinLock					m_signal_lock;
		static_assert(_SIGMAX < 64);

#if __enable_sse
		alignas(16) uint8_t m_sse_storage[512] {};
#endif

		friend class Scheduler;
	};

}
