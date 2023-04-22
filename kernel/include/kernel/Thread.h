#pragma once

#include <BAN/NoCopyMove.h>
#include <BAN/RefPtr.h>

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
		static BAN::ErrorOr<Thread*> create(entry_t, void*, Process*);
		static BAN::ErrorOr<Thread*> create_userspace(uintptr_t, Process*);
		~Thread();

		void jump_userspace(uintptr_t rip);

		pid_t tid() const { return m_tid; }

		void set_rsp(uintptr_t rsp) { m_rsp = rsp; validate_stack(); }
		void set_rip(uintptr_t rip) { m_rip = rip; }
		uintptr_t rsp() const { return m_rsp; }
		uintptr_t rip() const { return m_rip; }

		void set_started() { ASSERT(m_state == State::NotStarted); m_state = State::Executing; }
		State state() const { return m_state; }
		void terminate() { m_state = State::Terminating; }

		uintptr_t stack_base() const { return (uintptr_t)m_stack_base; }
		size_t stack_size() const { return m_stack_size; }

		static Thread& current() ;
		Process& process();
		bool has_process() const { return m_process; }

		void set_in_syscall(bool b) { m_in_syscall = b; }

	private:
		Thread(pid_t tid, Process*);

		void validate_stack() const { if (!m_in_syscall) ASSERT(stack_base() <= m_rsp && m_rsp <= stack_base() + stack_size()); }
		
		BAN::ErrorOr<void> initialize(entry_t, void*);
		void on_exit();
		
	private:
		static constexpr size_t m_stack_size = 4096 * 1;
		void*		m_stack_base	{ nullptr };
		uintptr_t	m_rip			{ 0 };
		uintptr_t	m_rsp			{ 0 };
		const pid_t	m_tid			{ 0 };
		State		m_state			{ State::NotStarted };
		Process*	m_process		{ nullptr };
		bool		m_in_syscall	{ false };

		friend class Scheduler;
	};
	
}