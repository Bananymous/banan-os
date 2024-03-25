#include <kernel/GDT.h>

namespace Kernel
{

	GDT* GDT::create()
	{
		ASSERT_NOT_REACHED();
	}

	void GDT::write_entry(uint8_t, uint32_t, uint32_t, uint8_t, uint8_t)
	{
		ASSERT_NOT_REACHED();
	}

	void GDT::write_tss()
	{
		ASSERT_NOT_REACHED();
	}

}
