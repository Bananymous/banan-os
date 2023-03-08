#pragma once

#include <BAN/Function.h>
#include <BAN/NoCopyMove.h>

namespace Kernel
{

	class Thread : public BAN::RefCounted<Thread>
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<Thread>> create(const BAN::Function<void()>&);
		~Thread();

		uint32_t tid() const { return m_tid; }

		void set_rsp(uintptr_t rsp) { m_rsp = rsp; }
		void set_rip(uintptr_t rip) { m_rip = rip; }
		uintptr_t rsp() const { return m_rsp; }
		uintptr_t rip() const { return m_rip; }

		void set_started() { m_started = true; }
		bool started() const { return m_started; }

		const BAN::Function<void()>* function() const { return &m_function; }

	private:
		Thread(const BAN::Function<void()>&);
		void on_exit();
		
	private:
		void*			m_stack_base	= nullptr;
		uintptr_t		m_rip			= 0;
		uintptr_t		m_rsp			= 0;
		const uint32_t	m_tid			= 0;
		bool			m_started		= false;
		
		BAN::Function<void()> m_function;

		friend class BAN::RefPtr<Thread>;
	};

}