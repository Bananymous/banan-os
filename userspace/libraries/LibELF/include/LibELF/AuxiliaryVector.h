#pragma once

#include <stdint.h>

namespace LibELF
{

	struct AuxiliaryVector
	{
		uint32_t a_type;
		union
		{
			uint32_t a_val;
			void* a_ptr;
		} a_un;
	};

	enum AuxiliaryVectorValues
	{
		AT_NULL   = 0,
		AT_IGNORE = 1,
		AT_EXECFD = 2,
		AT_PHDR   = 3,
		AT_PHENT  = 4,
		AT_PHNUM  = 5,

		AT_SHARED_PAGE = 0xFFFF0001,
	};

}
