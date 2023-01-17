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
		static constexpr void*(&allocator_align)(size_t, size_t) = kmalloc;
		static constexpr void(&deallocator)(void*) = kfree;
	#else
		static constexpr void*(&allocator)(size_t) = malloc;
		static constexpr void(&deallocator)(void*) = free;
	#endif

	template<typename T>
	class OwnPtr
	{
	public:
		template<typename... Args>
		OwnPtr(const Args&... args)
		{
			m_pointer = new T(args...);
		}

		~OwnPtr()
		{
			delete m_pointer;
		}

	private:
		T* m_pointer = nullptr;
	};

}

inline void* operator new(size_t, void* addr)	{ return addr; }
inline void* operator new[](size_t, void* addr)	{ return addr; }
