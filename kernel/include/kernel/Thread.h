#pragma once

#include <BAN/Memory.h>

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
			Done,
		};

	public:
		Thread(void(*)());
		~Thread();

		uint32_t id() const { return m_id; }

		void set_rip(uintptr_t rip) { m_rip = rip; }
		void set_rsp(uintptr_t rsp) { m_rsp = rsp; }
		void set_state(State state) { m_state = state; }
		uintptr_t rip() const { return m_rip; }
		uintptr_t rsp() const { return m_rsp; }
		State state() const { return m_state; }

	private:
		static void on_exit();
		
	private:
		void*			m_stack_base	= nullptr;
		State			m_state			= State::NotStarted;
		uintptr_t		m_rip			= 0;
		uintptr_t		m_rsp			= 0;
		const uint32_t	m_id			= 0;
	};


}