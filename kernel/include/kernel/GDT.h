#pragma once

#include <stdint.h>

namespace Kernel::GDT
{

	static constexpr inline bool is_user_segment(uint8_t segment)
	{
		return (segment & 3) == 3;
	}

	void initialize();
	void set_tss_stack(uintptr_t);

}