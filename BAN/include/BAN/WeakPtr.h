#pragma once

#include <BAN/RefPtr.h>

namespace BAN
{

	template<typename T>
	class Weakable;

	template<typename T>
	class WeakPtr;

	template<typename T>
	class WeakLink : public RefCounted<WeakLink<T>>
	{
	public:
		RefPtr<T> lock() { ASSERT(m_ptr); return raw_ptr(); }
		T* raw_ptr() { return m_ptr; }

		bool valid() const { return m_ptr; }
		void invalidate() { m_ptr = nullptr; }

	private:
		WeakLink(T* ptr) : m_ptr(ptr) {}

	private:
		T* m_ptr;

		friend class RefPtr<WeakLink<T>>;
	};

	template<typename T>
	class Weakable
	{
	public:
		~Weakable()
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

		RefPtr<T> lock()
		{
			if (m_link->valid())
				return m_link->lock();
			return nullptr;
		}

		void clear() { m_link.clear(); }

		bool valid() const { return m_link && m_link->valid(); }

		operator bool() const { return valid(); }

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
