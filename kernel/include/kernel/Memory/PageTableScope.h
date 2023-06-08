#pragma once

#include <kernel/CriticalScope.h>
#include <kernel/LockGuard.h>
#include <kernel/Memory/PageTable.h>

namespace Kernel
{

	class PageTableScope
	{
	public:
		PageTableScope(PageTable& page_table)
			: m_guard(page_table)
			, m_old(PageTable::current())
			, m_temp(page_table)
		{
			if (&m_old != &m_temp)
				m_temp.load();
		}
		~PageTableScope()
		{
			if (&m_old != &m_temp)
				m_old.load();
		}
	private:
		LockGuard<PageTable> m_guard;
		CriticalScope m_scope;
		PageTable& m_old;
		PageTable& m_temp;
	};

}