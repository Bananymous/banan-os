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
		static constexpr void(&deallocator)(void*) = kfree;
	#else
		static constexpr void*(&allocator)(size_t) = malloc;
		static constexpr void(&deallocator)(void*) = free;
	#endif

	template<typename T>
	class RefCounted
	{
		BAN_NON_COPYABLE(RefCounted);
		BAN_NON_MOVABLE(RefCounted);

	public:
		uint32_t ref_count() const
		{
			return m_ref_count;
		}

		void ref() const
		{
			ASSERT(m_ref_count > 0);
			m_ref_count++;
		}

		void unref() const
		{
			ASSERT(m_ref_count > 0);
			m_ref_count--;
			if (m_ref_count == 0)
				delete (const T*)this;
		}

	protected:
		RefCounted() = default;
		~RefCounted() { ASSERT(m_ref_count == 0); }

	private:
		mutable uint32_t m_ref_count = 1;
	};

	template<typename T>
	class RefPtr
	{
	public:
		RefPtr() = default;
		~RefPtr() { clear(); }

		template<typename U>
		static RefPtr adopt(U* pointer)
		{
			return RefPtr(pointer);
		}

		template<typename... Args>
		static ErrorOr<RefPtr> create(Args&&... args)
		{
			T* pointer = new T(forward<Args>(args)...);
			if (pointer == nullptr)
				return Error::from_errno(ENOMEM);
			return RefPtr(pointer);
		}

		RefPtr(const RefPtr& other) { *this = other; }
		RefPtr(RefPtr&& other) { *this = move(other); }

		RefPtr& operator=(const RefPtr& other)
		{
			clear();
			m_pointer = other.m_pointer;
			if (m_pointer)
				m_pointer->ref();
			return *this;
		}

		RefPtr& operator=(RefPtr&& other)
		{
			clear();
			m_pointer = other.m_pointer;
			other.m_pointer = nullptr;
			return *this;
		}

		T* ptr() { ASSERT(!empty()); return m_pointer; }
		const T* ptr() const { ASSERT(!empty()); return m_pointer; }

		T& operator*() { return *ptr(); }
		const T& operator*() const { return *ptr(); }

		T* operator->() { return ptr(); }
		const T* operator->() const { return ptr(); }

		bool empty() const { return m_pointer == nullptr; }
		operator bool() const { return m_pointer; }

		void clear()
		{
			if (m_pointer)
				m_pointer->unref();
			m_pointer = nullptr;
		}

	private:
		RefPtr(T* pointer)
			: m_pointer(pointer)
		{}

	private:
		T* m_pointer = nullptr;
	};

}

inline void* operator new(size_t, void* addr)	{ return addr; }
inline void* operator new[](size_t, void* addr)	{ return addr; }
