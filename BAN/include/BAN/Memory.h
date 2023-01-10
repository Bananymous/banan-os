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

inline void* operator new(size_t size)		{ return BAN::allocator(size); }
inline void* operator new[](size_t size)	{ return BAN::allocator(size); }

inline void operator delete(void* addr)				{ BAN::deallocator(addr); }
inline void operator delete[](void* addr)			{ BAN::deallocator(addr); }
inline void operator delete(void* addr, size_t)		{ BAN::deallocator(addr); }
inline void operator delete[](void* addr, size_t)	{ BAN::deallocator(addr); }