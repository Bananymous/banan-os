#pragma once

#include <BAN/Errors.h>
#include <BAN/Move.h>
#include <BAN/NoCopyMove.h>
#include <stdint.h>

namespace BAN
{

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
		RefPtr(T* pointer)
		{
			m_pointer = pointer;
			if (m_pointer)
				m_pointer->ref();
		}
		~RefPtr() { clear(); }

		template<typename U>
		static RefPtr adopt(U* pointer)
		{
			RefPtr ptr;
			ptr.m_pointer = pointer;
			return ptr;
		}

		template<typename... Args>
		static ErrorOr<RefPtr> create(Args&&... args)
		{
			T* pointer = new T(forward<Args>(args)...);
			if (pointer == nullptr)
				return Error::from_errno(ENOMEM);
			return adopt(pointer);
		}

		RefPtr(const RefPtr& other) { *this = other; }
		RefPtr(RefPtr&& other) { *this = move(other); }
		template<typename U>
		RefPtr(const RefPtr<U>& other) { *this = other; }
		template<typename U>
		RefPtr(RefPtr<U>&& other) { *this = move(other); }

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

		template<typename U>
		RefPtr& operator=(const RefPtr<U>& other)
		{
			clear();
			m_pointer = other.m_pointer;
			if (m_pointer)
				m_pointer->ref();
			return *this;
		}

		template<typename U>
		RefPtr& operator=(RefPtr<U>&& other)
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
		T* m_pointer = nullptr;

		template<typename U>
		friend class RefPtr;
	};

}
