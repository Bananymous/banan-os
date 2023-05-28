#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/RefPtr.h>
#include <kernel/Memory/VirtualRange.h>

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
		static BAN::ErrorOr<Thread*> create_userspace(uintptr_t, Process*, int, char**);
		~Thread();

		BAN::ErrorOr<Thread*> clone(Process*, uintptr_t rsp, uintptr_t rip);

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

	private:
		Thread(pid_t tid, Process*);
		void on_exit();
		
		void validate_stack() const;

	private:
		struct userspace_entry_t
		{
			uintptr_t entry;
			int argc  { 0 };
			char** argv  { 0 };
		};

	private:
		static constexpr size_t m_kernel_stack_size		= PAGE_SIZE * 1;
		static constexpr size_t m_userspace_stack_size	= PAGE_SIZE * 2;
		static constexpr size_t m_interrupt_stack_size	= PAGE_SIZE * 2;
		VirtualRange*	m_interrupt_stack	{ nullptr };
		VirtualRange*	m_stack				{ nullptr };
		uintptr_t		m_rip				{ 0 };
		uintptr_t		m_rsp				{ 0 };
		const pid_t		m_tid				{ 0 };
		State			m_state				{ State::NotStarted };
		Process*		m_process			{ nullptr };
		bool			m_in_syscall		{ false };
		bool			m_is_userspace		{ false };

		userspace_entry_t m_userspace_entry;

		friend class Scheduler;
	};
	
}