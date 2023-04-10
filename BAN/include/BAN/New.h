#pragma once

#if defined(__is_kernel)
	#include <kernel/kmalloc.h>
#else
	#include <stdlib.h>
#endif

namespace BAN
{
	#if defined(__is_kernel)
		static constexpr void*(&allocator)(size_t) = kmalloc;
		static constexpr void(&deallocator)(void*) = kfree;
	#else
		static constexpr void*(&allocator)(size_t) = malloc;
		static constexpr void(&deallocator)(void*) = free;
	#endif
}
