#pragma once

#include <BAN/NoCopyMove.h>

#include <stddef.h>

namespace Kernel
{

	class CriticalScope
	{
		BAN_NON_COPYABLE(CriticalScope);
		BAN_NON_MOVABLE(CriticalScope);

	public:
		CriticalScope()
		{
			asm volatile("pushf; cli; pop %0" : "=r"(m_flags) :: "memory");
		}

		~CriticalScope()
		{
			asm volatile("push %0; popf" :: "rm"(m_flags) : "memory", "cc");
		}

	private:
		size_t m_flags;
	};

}