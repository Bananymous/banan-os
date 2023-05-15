#pragma once

#include <kernel/CriticalScope.h>
#include <kernel/Memory/MMU.h>

namespace Kernel
{

	class MMUScope
	{
	public:
		MMUScope(MMU& mmu)
			: m_old(MMU::current())
			, m_temp(mmu)
		{
			if (&m_old != &m_temp)
				m_temp.load();
		}
		~MMUScope()
		{
			if (&m_old != &m_temp)
				m_old.load();
		}
	private:
		CriticalScope m_scope;
		MMU& m_old;
		MMU& m_temp;
	};

}