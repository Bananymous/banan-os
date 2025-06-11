#pragma once

#include <BAN/RefPtr.h>

#if __is_kernel
#include <kernel/Lock/SpinLock.h>
#endif

namespace BAN
{

	template<typename T>
	class Weakable;

	template<typename T>
	class WeakPtr;

	// FIXME: Write this without using locks...
	template<typename T>
	class WeakLink : public RefCounted<WeakLink<T>>
	{
	public:
		RefPtr<T> try_lock() const
		{
#if __is_kernel
			Kernel::SpinLockGuard _(m_weak_lock);
#endif
			if (m_ptr && m_ptr->try_ref())
				return RefPtr<T>::adopt(m_ptr);
			return nullptr;
		}
		bool valid() const { return m_ptr; }
		void invalidate()
		{
#if __is_kernel
			Kernel::SpinLockGuard _(m_weak_lock);
#endif
			m_ptr = nullptr;
		}

	private:
		WeakLink(T* ptr) : m_ptr(ptr) {}

	private:
		T* m_ptr;
#if __is_kernel
		mutable Kernel::SpinLock m_weak_lock;
#endif
		friend class RefPtr<WeakLink<T>>;
	};

	template<typename T>
	class Weakable
	{
	public:
		virtual ~Weakable()
		{
			if (m_link)
				m_link->invalidate();
		}

		ErrorOr<WeakPtr<T>> get_weak_ptr() const
		{
			if (!m_link)
				m_link = TRY(RefPtr<WeakLink<T>>::create((T*)this));
			return WeakPtr<T>(m_link);
		}

	private:
		mutable RefPtr<WeakLink<T>> m_link;
	};

	template<typename T>
	class WeakPtr
	{
	public:
		WeakPtr() = default;
		WeakPtr(WeakPtr&& other)		{ *this = move(other); }
		WeakPtr(const WeakPtr& other)	{ *this = other; }
		WeakPtr(const RefPtr<T>& other)	{ *this = other; }

		WeakPtr& operator=(WeakPtr&& other)
		{
			clear();
			m_link = move(other.m_link);
			return *this;
		}
		WeakPtr& operator=(const WeakPtr& other)
		{
			clear();
			m_link = other.m_link;
			return *this;
		}
		WeakPtr& operator=(const RefPtr<T>& other)
		{
			clear();
			if (other)
				m_link = MUST(other->get_weak_ptr()).move_link();
			return *this;
		}

		RefPtr<T> lock() const
		{
			if (m_link)
				return m_link->try_lock();
			return nullptr;
		}

		void clear() { m_link.clear(); }

		bool valid() const { return m_link && m_link->valid(); }

		explicit operator bool() const { return valid(); }

	private:
		WeakPtr(const RefPtr<WeakLink<T>>& link)
			: m_link(link)
		{ }

		RefPtr<WeakLink<T>>&& move_link() { return move(m_link); }

	private:
		RefPtr<WeakLink<T>> m_link;

		friend class Weakable<T>;
	};

}
