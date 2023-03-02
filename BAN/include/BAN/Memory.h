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
		RefCounted() = default;
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
			clear();
		}
		
		template<typename U>
		static ErrorOr<RefCounted<T>> adopt(U* data)
		{
			uint32_t* count = new uint32_t(1);
			if (!count)
				return Error::from_string("RefCounted: Could not allocate memory");
			return RefCounted<T>((T*)data, count);
		}

		template<typename... Args>
		static ErrorOr<RefCounted<T>> create(Args... args)
		{
			uint32_t* count = new uint32_t(1);
			if (!count)
				return Error::from_string("RefCounted: Could not allocate memory");
			T* data = new T(forward<Args>(args)...);
			if (!data)
				return Error::from_string("RefCounted: Could not allocate memory");
			return RefCounted<T>(data, count);
		}

		RefCounted<T>& operator=(const RefCounted<T>& other)
		{
			clear();
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
			clear();
			if (other)
			{
				m_pointer = other.m_pointer;
				m_count = other.m_count;
				other.m_pointer = nullptr;
				other.m_count = nullptr;
			}
			return *this;
		}

		T* ptr() { return m_pointer; }
		const T* ptr() const { return m_pointer; }

		T& operator*() { return *ptr();}
		const T& operator*() const { return *ptr();}

		T* operator->() { return ptr(); }
		const T* operator->() const { return ptr(); }

		void clear()
		{
			if (!*this)
				return;

			(*m_count)--;
			if (*m_count == 0)
			{
				delete m_pointer;
				delete m_count;
			}

			m_pointer = nullptr;
			m_count = nullptr;
		}

		operator bool() const
		{
			if (!m_count && !m_pointer)
				return false;
			ASSERT(m_count && m_pointer);
			ASSERT(*m_count > 0);
			return true;
		}
	
	private:
		RefCounted(T* pointer, uint32_t* count)
			: m_pointer(pointer)
			, m_count(count)
		{
			ASSERT(!pointer == !count);
		}

	private:
		T*			m_pointer = nullptr;
		uint32_t*	m_count = nullptr;
	};

}

inline void* operator new(size_t, void* addr)	{ return addr; }
inline void* operator new[](size_t, void* addr)	{ return addr; }
