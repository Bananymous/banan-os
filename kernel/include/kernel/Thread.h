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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpmf-conversions"
		template<typename... Args>
		Thread(const BAN::Function<void(Args...)>& func, Args... args)
			: Thread((uintptr_t)(void*)&BAN::Function<void(Args...)>::operator(), (uintptr_t)&func, ((uintptr_t)args)...)
		{
			static_assert(((BAN::is_integral_v<Args> || BAN::is_pointer_v<Args>) && ...));
		}
#pragma GCC diagnostic pop

		~Thread();

		uint32_t id() const { return m_id; }

		void set_rsp(uintptr_t rsp) { m_rsp = rsp; }
		void set_rip(uintptr_t rip) { m_rip = rip; }
		void set_state(State state) { m_state = state; }
		uintptr_t rsp() const { return m_rsp; }
		uintptr_t rip() const { return m_rip; }
		State state() const { return m_state; }

		const uintptr_t* args() const { return m_args; }

	private:
		Thread(uintptr_t rip, uintptr_t func, uintptr_t arg1 = 0, uintptr_t arg2 = 0, uintptr_t arg3 = 0);
		void on_exit();
		
	private:
		void*			m_stack_base	= nullptr;
		State			m_state			= State::NotStarted;
		uintptr_t		m_args[4]		= {};
		uintptr_t		m_rip			= 0;
		uintptr_t		m_rsp			= 0;
		const uint32_t	m_id			= 0;
		
		alignas(max_align_t) uint8_t m_function[BAN::Function<void()>::size()] { 0 };
	};

}