#pragma once


#include <BAN/Errors.h>
#include <BAN/Move.h>
#include <BAN/NoCopyMove.h>

#if defined(__is_kernel)
	#include <kernel/kmalloc.h>
#else
	#include <stdlib.h>
#endif

#include <stdint.h>

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
	class Unique
	{
		BAN_NON_COPYABLE(Unique);

	public:
		template<typename... Args>
		Unique(const Args&... args)
		{
			m_pointer = new T(args...);
		}

		~Unique()
		{
			delete m_pointer;
		}

		operator bool() const
		{
			return m_pointer;
		}

	private:
		T* m_pointer = nullptr;
	};

	template<typename T>
	class RefCounted
	{
	public:
		RefCounted() { }
		RefCounted(T* pointer)
		{
			if (pointer)
			{
				m_pointer = pointer;
				m_count = new int32_t(1);
				ASSERT(m_count);
			}
		}
		RefCounted(const RefCounted<T>& other)
		{
			*this = other;
		}
		RefCounted(RefCounted<T>&& other)
		{
			*this = move(other);
		}
		~RefCounted()
		{
			reset();
		}
		
		template<typename... Args>
		static RefCounted<T> create(Args... args)
		{
			return RefCounted<T>(new T(forward<Args>(args)...), new int32_t(1));
		}

		RefCounted<T>& operator=(const RefCounted<T>& other)
		{
			reset();
			if (other)
			{
				m_pointer = other.m_pointer;
				m_count = other.m_count;
				(*m_count)++;
			}
			return *this;
		}
		
		RefCounted<T>& operator=(RefCounted<T>&& other)
		{
			reset();
			m_pointer = other.m_pointer;
			m_count = other.m_count;
			other.m_pointer = nullptr;
			other.m_count = nullptr;
			if (!(*this))
				reset();
			return *this;
		}

		T& operator*() { return *m_pointer;}
		const T& operator*() const { return *m_pointer;}

		T* operator->() { return m_pointer; }
		const T* operator->() const { return m_pointer; }

		void reset()
		{
			ASSERT(!m_count == !m_pointer);
			if (!m_count)
				return;
			(*m_count)--;
			if (*m_count == 0)
			{
				delete m_count;
				delete m_pointer;
			}
			m_count = nullptr;
			m_pointer = nullptr;
		}

		operator bool() const
		{
			ASSERT(!m_count == !m_pointer);
			return m_count && *m_count > 0;
		}

		bool operator==(const RefCounted<T>& other) const
		{
			if (m_pointer != other.m_pointer)
				return false;
			ASSERT(m_count == other.m_count);
			return !m_count || *m_count > 0;
		}
	
	private:
		RefCounted(T* pointer, int32_t* count)
			: m_pointer(pointer)
			, m_count(count)
		{
			ASSERT(!pointer == !count);
		}

	private:
		T*			m_pointer = nullptr;
		int32_t*	m_count = nullptr;
	};

}

inline void* operator new(size_t, void* addr)	{ return addr; }
inline void* operator new[](size_t, void* addr)	{ return addr; }
