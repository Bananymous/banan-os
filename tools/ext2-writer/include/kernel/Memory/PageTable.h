#pragma once

#include <kernel/Memory/Types.h>

namespace Kernel
{

	class PageTable
	{
	public:
		using flags_t = uint16_t;
		enum Flags : flags_t
		{
			Present			= (1 << 0),
			ReadWrite		= (1 << 1),
			UserSupervisor	= (1 << 2),
			Reserved		= (1 << 9),

			Execute			= (1 << 15),
			Used = Present | Reserved,
		};

	public:
		static PageTable& kernel()
		{
			static PageTable s_instance;
			return s_instance;
		}

	private:
		PageTable() = default;
	};

}
