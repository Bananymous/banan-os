#pragma once

#include <BAN/Errors.h>
#include <BAN/Memory.h>
#include <BAN/Move.h>

namespace BAN
{

	template<typename T>
	class LinkedListIterator;
	template<typename T>
	class LinkedListConstIterator;
	
	template<typename T>
	class LinkedList
	{
		BAN_NON_COPYABLE(LinkedList<T>);
		BAN_NON_MOVABLE(LinkedList<T>);

	public:
		using size_type = size_t;
		using value_type = T;
		using iterator = LinkedListIterator<T>;
		using const_iterator = LinkedListConstIterator<T>;

	public:
		LinkedList() = default;
		~LinkedList();

		[[nodiscard]] ErrorOr<void> PushBack(const T&);
		[[nodiscard]] ErrorOr<void> PushBack(T&&);
		[[nodiscard]] ErrorOr<void> Insert(const_iterator, const T&);
		[[nodiscard]] ErrorOr<void> Insert(const_iterator, T&&);
		template<typename... Args>
		[[nodiscard]] ErrorOr<void> EmplaceBack(Args...);
		template<typename... Args>
		[[nodiscard]] ErrorOr<void> Emplace(const_iterator, Args...);

		void PopBack();
		void Remove(const_iterator);
		void Clear();

		iterator begin()				{ return iterator(m_data, false); }
		const_iterator begin() const	{ return const_iterator(m_data, false); }
		iterator end()					{ return iterator(m_last, true); }
		const_iterator end() const		{ return const_iterator(m_last, true); }

		const T& Back() const;
		T& Back();
		const T& Front() const;
		T& Front();

		size_type Size() const;
		bool Empty() const;

	private:
		struct Node
		{
			T value;
			Node* next;
			Node* prev;
		};

		[[nodiscard]] ErrorOr<Node*> allocate_node() const;

		Node* m_data = nullptr;
		Node* m_last = nullptr;
		size_type m_size = 0;

		friend class LinkedListIterator<T>;
		friend class LinkedListConstIterator<T>;
	};

	template<typename T>
	class LinkedListIterator
	{
	public:
		using value_type = T;

	public:
		LinkedListIterator() = default;
		LinkedListIterator(const LinkedListIterator<T>& other) : m_current(other.m_current), m_past_end(other.m_past_end) {}
		LinkedListIterator<T>& operator++()
		{
			ASSERT(m_current);
			ASSERT(m_current->next || !m_past_end);
			if (m_current->next)
				m_current = m_current->next;
			else
				m_past_end = true;
			return *this;
		}
		LinkedListIterator<T>& operator--()
		{
			ASSERT(m_current);
			ASSERT(m_current->prev || m_past_end);
			if (m_past_end)
				m_past_end = false;
			else
				m_current = m_current->prev;
			return *this;
		}
		LinkedListIterator<T> operator++(int)						{ auto temp = *this; ++(*this); return temp; }
		LinkedListIterator<T> operator--(int)						{ auto temp = *this; --(*this); return temp; }
		T& operator*()												{ ASSERT(m_current); return m_current->value; }
		const T& operator*() const									{ ASSERT(m_current); return m_current->value; }
		bool operator==(const LinkedListIterator<T>& other) const	{ return m_current && m_current == other.m_current && m_past_end == other.m_past_end; }
		bool operator!=(const LinkedListIterator<T>& other) const	{ return !(*this == other); }
		operator bool() const										{ return m_current; }
	private:
		LinkedListIterator(typename LinkedList<T>::Node* node, bool past_end) : m_current(node), m_past_end(past_end) { }
	private:
		typename LinkedList<T>::Node* m_current = nullptr;
		bool m_past_end = false;

		friend class LinkedList<T>;
		friend class LinkedListConstIterator<T>;
	};

	template<typename T>
	class LinkedListConstIterator
	{
	public:
		using value_type = T;

	public:
		LinkedListConstIterator() = default;
		LinkedListConstIterator(const LinkedListIterator<T>& other)			: m_current(other.m_current), m_past_end(other.m_past_end) {}
		LinkedListConstIterator(const LinkedListConstIterator<T>& other)	: m_current(other.m_current), m_past_end(other.m_past_end) {}
		LinkedListConstIterator<T>& operator++()
		{
			ASSERT(m_current);
			ASSERT(m_current->next || !m_past_end);
			if (m_current->next)
				m_current = m_current->next;
			else
				m_past_end = true;
			return *this;
		}
		LinkedListConstIterator<T>& operator--()
		{
			ASSERT(m_current);
			ASSERT(m_current->prev || m_past_end);
			if (m_past_end)
				m_past_end = false;
			else
				m_current = m_current->prev;
			return *this;
		}
		LinkedListConstIterator<T> operator++(int)						{ auto temp = *this; ++(*this); return temp; }
		LinkedListConstIterator<T> operator--(int)						{ auto temp = *this; --(*this); return temp; }
		const T& operator*() const										{ ASSERT(m_current); return m_current->value; }
		bool operator==(const LinkedListConstIterator<T>& other) const	{ return m_current && m_current == other.m_current && m_past_end == other.m_past_end; }
		bool operator!=(const LinkedListConstIterator<T>& other) const	{ return !(*this == other); }
		operator bool() const											{ return m_current; }
	private:
		LinkedListConstIterator(typename LinkedList<T>::Node* node, bool past_end) : m_current(node), m_past_end(past_end) {}
	private:
		typename LinkedList<T>::Node* m_current = nullptr;
		bool m_past_end = false;

		friend class LinkedList<T>;
	};

	template<typename T>
	LinkedList<T>::~LinkedList()
	{
		Clear();
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::PushBack(const T& value)
	{
		return PushBack(Move(T(value)));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::PushBack(T&& value)
	{
		return Insert(end(), Move(value));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::Insert(const_iterator iter, const T& value)
	{
		return Insert(iter, Move(T(value)));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::Insert(const_iterator iter, T&& value)
	{
		Node* next = iter.m_past_end ? nullptr : iter.m_current;
		Node* prev = next ? next->prev : m_last;
		Node* new_node = TRY(allocate_node());
		new (&new_node->value) T(Move(value));
		new_node->next = next;
		new_node->prev = prev;
		(prev ? prev->next : m_data) = new_node;
		(next ? next->prev : m_last) = new_node;
		m_size++;
		return {};
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> LinkedList<T>::EmplaceBack(Args... args)
	{
		return Emplace(end(), Forward<Args>(args)...);
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> LinkedList<T>::Emplace(const_iterator iter, Args... args)
	{
		Node* next = iter.m_past_end ? nullptr : iter.m_current;
		Node* prev = next ? next->prev : m_last;
		Node* new_node = TRY(allocate_node());
		new (&new_node->value) T(Forward<Args>(args)...);
		new_node->next = next;
		new_node->prev = prev;
		(prev ? prev->next : m_data) = new_node;
		(next ? next->prev : m_last) = new_node;
		m_size++;
		return {};
	}

	template<typename T>
	void LinkedList<T>::PopBack()
	{
		return Remove(m_last);
	}

	template<typename T>
	void LinkedList<T>::Remove(const_iterator iter)
	{
		ASSERT(m_size > 0);
		Node* node = iter.m_current;
		Node* prev = node->prev;
		Node* next = node->next;
		node->value.~T();
		BAN::deallocator(node);
		(prev ? prev->next : m_data) = next;
		(next ? next->prev : m_last) = prev;
		m_size--;
	}

	template<typename T>
	void LinkedList<T>::Clear()
	{
		Node* ptr = m_data;
		while (ptr)
		{
			Node* next = ptr->next;
			ptr->value.~T();
			BAN::deallocator(ptr);
			ptr = next;
		}
		m_data = nullptr;
		m_last = nullptr;
		m_size = 0;
	}

	template<typename T>
	const T& LinkedList<T>::Back() const
	{
		ASSERT(m_size > 0);
		return *const_iterator(m_last);
	}

	template<typename T>
	T& LinkedList<T>::Back()
	{
		ASSERT(m_size > 0);
		return *iterator(m_last);
	}

	template<typename T>
	const T& LinkedList<T>::Front() const
	{
		ASSERT(m_size > 0);
		return *const_iterator(m_data);
	}

	template<typename T>
	T& LinkedList<T>::Front()
	{
		ASSERT(m_size > 0);
		return *iterator(m_data);
	}

	template<typename T>
	typename LinkedList<T>::size_type LinkedList<T>::Size() const
	{
		return m_size;
	}

	template<typename T>
	bool LinkedList<T>::Empty() const
	{
		return m_size == 0;
	}

	template<typename T>
	ErrorOr<typename LinkedList<T>::Node*> LinkedList<T>::allocate_node() const
	{
		Node* node = (Node*)BAN::allocator(sizeof(Node));
		if (node == nullptr)
			return Error::FromString("LinkedList: Could not allocate memory");	
		return node;
	}

}