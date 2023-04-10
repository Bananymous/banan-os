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
		static BAN::ErrorOr<Thread*> create(entry_t, void* = nullptr, BAN::RefPtr<Process> = nullptr);
		~Thread();

		pid_t tid() const { return m_tid; }

		void set_rsp(uintptr_t rsp) { m_rsp = rsp; }
		void set_rip(uintptr_t rip) { m_rip = rip; }
		uintptr_t rsp() const { return m_rsp; }
		uintptr_t rip() const { return m_rip; }

		void set_started() { ASSERT(m_state == State::NotStarted); m_state = State::Executing; }
		State state() const { return m_state; }
		void terminate() { m_state = State::Terminating; }

		static Thread& current() ;
		BAN::RefPtr<Process> process();

	private:
		Thread(pid_t tid, BAN::RefPtr<Process>);

		BAN::ErrorOr<void> initialize(entry_t, void*);
		void on_exit();
		
	private:
		void*		m_stack_base	= nullptr;
		uintptr_t	m_rip			= 0;
		uintptr_t	m_rsp			= 0;
		const pid_t	m_tid			= 0;
		State		m_state { State::NotStarted };
		BAN::RefPtr<Process> m_process;

		friend class Scheduler;
	};
	
}