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
			Terminating,
			Terminated
		};

		class TerminateBlocker
		{
		public:
			TerminateBlocker(Thread&);
			~TerminateBlocker();
		private:
			Thread& m_thread;
		};

	public:
		static BAN::ErrorOr<Thread*> create_kernel(entry_t, void*, Process*);
		static BAN::ErrorOr<Thread*> create_userspace(Process*);
		~Thread();

		BAN::ErrorOr<Thread*> clone(Process*, uintptr_t rsp, uintptr_t rip);
		void setup_exec();
		void setup_process_cleanup();

		bool has_signal_to_execute() const;
		void set_signal_done(int signal);
		void handle_signal(int signal = 0);
		bool add_signal(int signal);

		void set_return_rsp(uintptr_t& rsp) { m_return_rsp = &rsp; }
		void set_return_rip(uintptr_t& rip) { m_return_rip = &rip; }
		uintptr_t& return_rsp() { ASSERT(m_return_rsp); return *m_return_rsp; }
		uintptr_t& return_rip() { ASSERT(m_return_rip); return *m_return_rip; }

		pid_t tid() const { return m_tid; }

		void set_rsp(uintptr_t rsp) { m_rsp = rsp; validate_stack(); }
		void set_rip(uintptr_t rip) { m_rip = rip; }
		uintptr_t rsp() const { return m_rsp; }
		uintptr_t rip() const { return m_rip; }

		void set_started() { ASSERT(m_state == State::NotStarted); m_state = State::Executing; }
		void set_terminating();
		State state() const { return m_state; }

		vaddr_t stack_base() const { return m_stack->vaddr(); }
		size_t stack_size() const { return m_stack->size(); }
		VirtualRange& stack() { return *m_stack; }

		vaddr_t interrupt_stack_base() const { return m_interrupt_stack ? m_interrupt_stack->vaddr() : 0; }
		size_t interrupt_stack_size() const { return m_interrupt_stack ? m_interrupt_stack->size() : 0; }

		static Thread& current() ;
		static pid_t current_tid();

		Process& process();
		bool has_process() const { return m_process; }

		bool is_userspace() const { return m_is_userspace; }

		size_t virtual_page_count() const { return m_stack->size() / PAGE_SIZE; }

#if __enable_sse
		void save_sse() { asm volatile("fxsave %0" :: "m"(m_sse_storage)); }
		void load_sse() { asm volatile("fxrstor %0" :: "m"(m_sse_storage)); }
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
		uintptr_t					m_rip				{ 0 };
		uintptr_t					m_rsp				{ 0 };
		const pid_t					m_tid				{ 0 };
		State						m_state				{ State::NotStarted };
		Process*					m_process			{ nullptr };
		bool						m_is_userspace		{ false };

		uintptr_t*					m_return_rsp		{ nullptr };
		uintptr_t*					m_return_rip		{ nullptr };

		uint64_t					m_signal_pending_mask	{ 0 };
		uint64_t					m_signal_block_mask		{ 0 };
		int							m_handling_signal		{ 0 };
		static_assert(_SIGMAX < 64);

		uint64_t m_terminate_blockers { 0 };

#if __enable_sse
		alignas(16) uint8_t m_sse_storage[512] {};
#endif

		friend class Scheduler;
	};

}