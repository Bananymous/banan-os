#pragma once

#if defined(__is_kernel)
	#include <kernel/kmalloc.h>
#else
	#include <stdlib.h>
#endif

namespace BAN
{
	#if defined(__is_kernel)
		static constexpr auto& allocator = kmalloc;
		static constexpr auto& deallocator = kfree;
	#else
		static constexpr auto& allocator = malloc;
		static constexpr auto& deallocator = free;
	#endif
}