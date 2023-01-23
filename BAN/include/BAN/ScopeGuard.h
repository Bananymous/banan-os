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
			m_func();
		}
	private:
		BAN::Function<void()> m_func;
	};

}