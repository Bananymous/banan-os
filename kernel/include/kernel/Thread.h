#pragma once

#include <BAN/Function.h>
#include <BAN/NoCopyMove.h>

namespace Kernel
{

	class Thread
	{
		BAN_NON_COPYABLE(Thread);
		BAN_NON_MOVABLE(Thread);

	public:
		enum class State
		{
			NotStarted,
			Running,
			Paused,
			Sleeping,
			Done,
		};

	public:
		Thread(const BAN::Function<void()>&);
		~Thread();

		uint32_t tid() const { return m_tid; }

		void set_rsp(uintptr_t rsp) { m_rsp = rsp; }
		void set_rip(uintptr_t rip) { m_rip = rip; }
		void set_state(State state) { m_state = state; }
		uintptr_t rsp() const { return m_rsp; }
		uintptr_t rip() const { return m_rip; }
		State state() const { return m_state; }

		const BAN::Function<void()>* function() const { return &m_function; }

	private:
		void on_exit();
		
	private:
		void*			m_stack_base	= nullptr;
		State			m_state			= State::NotStarted;
		uintptr_t		m_rip			= 0;
		uintptr_t		m_rsp			= 0;
		const uint32_t	m_tid			= 0;
		
		BAN::Function<void()> m_function;
	};

}