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

		[[nodiscard]] ErrorOr<void> push_back(const T&);
		[[nodiscard]] ErrorOr<void> push_back(T&&);
		[[nodiscard]] ErrorOr<void> insert(const_iterator, const T&);
		[[nodiscard]] ErrorOr<void> insert(const_iterator, T&&);
		template<typename... Args>
		[[nodiscard]] ErrorOr<void> emplace_back(Args...);
		template<typename... Args>
		[[nodiscard]] ErrorOr<void> emplace(const_iterator, Args...);

		void pop_back();
		void remove(const_iterator);
		void clear();

		iterator begin()				{ return iterator(m_data, false); }
		const_iterator begin() const	{ return const_iterator(m_data, false); }
		iterator end()					{ return iterator(m_last, true); }
		const_iterator end() const		{ return const_iterator(m_last, true); }

		const T& back() const;
		T& back();
		const T& front() const;
		T& front();

		size_type size() const;
		bool empty() const;

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
		clear();
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::push_back(const T& value)
	{
		return push_back(Move(T(value)));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::push_back(T&& value)
	{
		return insert(end(), Move(value));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::insert(const_iterator iter, const T& value)
	{
		return insert(iter, Move(T(value)));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::insert(const_iterator iter, T&& value)
	{
		Node* next = iter.m_past_end ? nullptr : iter.m_current;
		Node* prev = next ? next->prev : m_last;
		Node* new_node = TRY(allocate_node());
		new (&new_node->value) T(move(value));
		new_node->next = next;
		new_node->prev = prev;
		(prev ? prev->next : m_data) = new_node;
		(next ? next->prev : m_last) = new_node;
		m_size++;
		return {};
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> LinkedList<T>::emplace_back(Args... args)
	{
		return emplace(end(), forward<Args>(args)...);
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> LinkedList<T>::emplace(const_iterator iter, Args... args)
	{
		Node* next = iter.m_past_end ? nullptr : iter.m_current;
		Node* prev = next ? next->prev : m_last;
		Node* new_node = TRY(allocate_node());
		new (&new_node->value) T(forward<Args>(args)...);
		new_node->next = next;
		new_node->prev = prev;
		(prev ? prev->next : m_data) = new_node;
		(next ? next->prev : m_last) = new_node;
		m_size++;
		return {};
	}

	template<typename T>
	void LinkedList<T>::pop_back()
	{
		return remove(m_last);
	}

	template<typename T>
	void LinkedList<T>::remove(const_iterator iter)
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
	void LinkedList<T>::clear()
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
	const T& LinkedList<T>::back() const
	{
		ASSERT(m_size > 0);
		return *const_iterator(m_last);
	}

	template<typename T>
	T& LinkedList<T>::back()
	{
		ASSERT(m_size > 0);
		return *iterator(m_last);
	}

	template<typename T>
	const T& LinkedList<T>::front() const
	{
		ASSERT(m_size > 0);
		return *const_iterator(m_data);
	}

	template<typename T>
	T& LinkedList<T>::front()
	{
		ASSERT(m_size > 0);
		return *iterator(m_data);
	}

	template<typename T>
	typename LinkedList<T>::size_type LinkedList<T>::size() const
	{
		return m_size;
	}

	template<typename T>
	bool LinkedList<T>::empty() const
	{
		return m_size == 0;
	}

	template<typename T>
	ErrorOr<typename LinkedList<T>::Node*> LinkedList<T>::allocate_node() const
	{
		Node* node = (Node*)BAN::allocator(sizeof(Node));
		if (node == nullptr)
			return Error::from_string("LinkedList: Could not allocate memory");	
		return node;
	}

}