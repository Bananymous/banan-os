#pragma once

#include <kernel/Processor.h>

namespace Kernel
{

	class CriticalScope
	{
	public:
		CriticalScope()
			: m_state(Processor::get_interrupt_state())
		{
			Processor::set_interrupt_state(InterruptState::Disabled);
		}

		~CriticalScope()
		{
			if (m_state != InterruptState::Disabled)
				Processor::set_interrupt_state(m_state);
		}

	private:
		const InterruptState m_state;
	};

}
