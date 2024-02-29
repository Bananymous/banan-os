#pragma once

#include <kernel/Arch.h>

namespace Kernel
{

	enum class InterruptState
	{
		Disabled,
		Enabled,
	};

#if ARCH(x86_64) || ARCH(i386)

	inline void set_interrupt_state(InterruptState state)
	{
		if (state == InterruptState::Enabled)
			asm volatile("sti");
		else
			asm volatile("cli");
	}

	inline InterruptState get_interrupt_state()
	{
		uintptr_t flags;
		asm volatile("pushf; pop %0" : "=rm"(flags));
		if (flags & (1 << 9))
			return InterruptState::Enabled;
		return InterruptState::Disabled;
	}

#else
#error "Unknown architecure"
#endif

}
