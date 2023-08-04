#pragma once

#include <BAN/Errors.h>
#include <BAN/NoCopyMove.h>

namespace BAN
{

	template<typename T>
	class UniqPtr
	{
		BAN_NON_COPYABLE(UniqPtr);

	public:
		UniqPtr() = default;

		template<typename U>
		UniqPtr(UniqPtr<U>&& other)
		{
			m_pointer = other.m_pointer;
			other.m_pointer = nullptr;
		}

		~UniqPtr()
		{
			clear();
		}

		static UniqPtr adopt(T* pointer)
		{
			UniqPtr uniq;
			uniq.m_pointer = pointer;
			return uniq;
		}

		template<typename... Args>
		static BAN::ErrorOr<UniqPtr> create(Args&&... args)
		{
			UniqPtr uniq;
			uniq.m_pointer = new T(BAN::forward<Args>(args)...);
			if (uniq.m_pointer == nullptr)
				return BAN::Error::from_errno(ENOMEM);
			return uniq;
		}

		template<typename U>
		UniqPtr& operator=(UniqPtr<U>&& other)
		{
			clear();
			m_pointer = other.m_pointer;
			other.m_pointer = nullptr;
			return *this;
		}

		T& operator*()
		{
			ASSERT(m_pointer);
			return *m_pointer;
		}

		const T& operator*() const
		{
			ASSERT(m_pointer);
			return *m_pointer;
		}

		T* operator->()
		{
			ASSERT(m_pointer);
			return m_pointer;	
		}

		const T* operator->() const
		{
			ASSERT(m_pointer);
			return m_pointer;
		}

		T* ptr() { return m_pointer; }
		const T* ptr() const { return m_pointer; }

		void clear()
		{
			if (m_pointer)
				delete m_pointer;
			m_pointer = nullptr;
		}

		operator bool() const { return m_pointer != nullptr; }

	private:
		T* m_pointer = nullptr;

		template<typename U>
		friend class UniqPtr;
	};

}