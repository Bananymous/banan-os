#pragma once

#include <BAN/CircularQueue.h>
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
		};

	public:
		static BAN::ErrorOr<Thread*> create_kernel(entry_t, void*, Process*);
		static BAN::ErrorOr<Thread*> create_userspace(Process*);
		~Thread();

		BAN::ErrorOr<Thread*> clone(Process*, uintptr_t rsp, uintptr_t rip);
		void setup_exec();

		void handle_next_signal();
		void handle_signal(int signal, uintptr_t& return_rsp, uintptr_t& return_rip);

		pid_t tid() const { return m_tid; }

		void set_rsp(uintptr_t rsp) { m_rsp = rsp; validate_stack(); }
		void set_rip(uintptr_t rip) { m_rip = rip; }
		uintptr_t rsp() const { return m_rsp; }
		uintptr_t rip() const { return m_rip; }

		void set_started() { ASSERT(m_state == State::NotStarted); m_state = State::Executing; }
		State state() const { return m_state; }
		void terminate() { m_state = State::Terminating; }

		vaddr_t stack_base() const { return m_stack->vaddr(); }
		size_t stack_size() const { return m_stack->size(); }

		vaddr_t interrupt_stack_base() const { return m_interrupt_stack ? m_interrupt_stack->vaddr() : 0; }
		size_t interrupt_stack_size() const { return m_interrupt_stack ? m_interrupt_stack->size() : 0; }

		static Thread& current() ;
		static pid_t current_tid();

		Process& process();
		bool has_process() const { return m_process; }

		void set_in_syscall(bool b) { m_in_syscall = b; }

		bool is_userspace() const { return m_is_userspace; }
		bool is_in_syscall() const { return m_in_syscall; }

	private:
		Thread(pid_t tid, Process*);
		void on_exit();
		
		void validate_stack() const;

	private:
		static constexpr size_t		m_kernel_stack_size		= PAGE_SIZE * 1;
		static constexpr size_t		m_userspace_stack_size	= PAGE_SIZE * 2;
		static constexpr size_t		m_interrupt_stack_size	= PAGE_SIZE * 2;
		BAN::UniqPtr<VirtualRange>	m_interrupt_stack;
		BAN::UniqPtr<VirtualRange>	m_stack;
		uintptr_t					m_rip				{ 0 };
		uintptr_t					m_rsp				{ 0 };
		const pid_t					m_tid				{ 0 };
		State						m_state				{ State::NotStarted };
		Process*					m_process			{ nullptr };
		bool						m_in_syscall		{ false };
		bool						m_is_userspace		{ false };

		BAN::CircularQueue<int, 10> m_signal_queue;
		vaddr_t m_signal_handlers[_SIGMAX + 1] { };
		uint64_t m_signal_mask { (1ull << SIGCHLD) | (1ull << SIGURG) };
		static_assert(_SIGMAX < 64);

		friend class Process;
		friend class Scheduler;
	};
	
}