#pragma once

#include <BAN/Function.h>

namespace BAN
{

	class ScopeGuard
	{
	public:
		ScopeGuard(const BAN::Function<void()>& func)
			: m_func(func)
		{ }
		~ScopeGuard()
		{
			if (m_enabled)
				m_func();
		}
		void disable()
		{
			m_enabled = false;
		}
	private:
		BAN::Function<void()> m_func;
		bool m_enabled { true };
	};

}