#pragma once

#include <BAN/Errors.h>
#include <BAN/Move.h>
#include <BAN/New.h>
#include <BAN/PlacementNew.h>

namespace BAN
{

	template<typename T, bool CONST>
	class LinkedListIterator;

	template<typename T>
	class LinkedList
	{
	public:
		using size_type = size_t;
		using value_type = T;
		using iterator = LinkedListIterator<T, false>;
		using const_iterator = LinkedListIterator<T, true>;

	public:
		LinkedList() = default;
		LinkedList(const LinkedList<T>& other) requires is_copy_constructible_v<T> { *this = other; }
		LinkedList(LinkedList<T>&& other)		{ *this = move(other); }
		~LinkedList()							{ clear(); }

		LinkedList<T>& operator=(const LinkedList<T>&) requires is_copy_constructible_v<T>;
		LinkedList<T>& operator=(LinkedList<T>&&);

		ErrorOr<void> push_back(const T&);
		ErrorOr<void> push_back(T&&);
		ErrorOr<void> insert(iterator, const T&);
		ErrorOr<void> insert(iterator, T&&);
		template<typename... Args>
		ErrorOr<void> emplace_back(Args&&...);
		template<typename... Args>
		ErrorOr<void> emplace(iterator, Args&&...);

		void pop_back();
		iterator remove(iterator);
		void clear();

		iterator begin()				{ return iterator(m_data, empty()); }
		const_iterator begin() const	{ return const_iterator(m_data, empty()); }
		iterator end()					{ return iterator(m_last, true); }
		const_iterator end() const		{ return const_iterator(m_last, true); }

		const T& back() const;
		T& back();
		const T& front() const;
		T& front();

		bool contains(const T&) const;

		size_type size() const;
		bool empty() const;

	private:
		struct Node
		{
			T value;
			Node* next;
			Node* prev;
		};

		ErrorOr<Node*> allocate_node() const;

		Node* m_data = nullptr;
		Node* m_last = nullptr;
		size_type m_size = 0;

		friend class LinkedListIterator<T, true>;
		friend class LinkedListIterator<T, false>;
	};

	template<typename T, bool CONST>
	class LinkedListIterator
	{
	public:
		using value_type = T;
		using data_type = maybe_const_t<CONST, typename LinkedList<T>::Node>;

	public:
		LinkedListIterator() = default;
		template<bool C>
		LinkedListIterator(const LinkedListIterator<T, C>&, enable_if_t<C == CONST || !C>* = 0);

		LinkedListIterator<T, CONST>& operator++();
		LinkedListIterator<T, CONST>& operator--();
		LinkedListIterator<T, CONST> operator++(int);
		LinkedListIterator<T, CONST> operator--(int);

		template<bool ENABLE = !CONST>
		enable_if_t<ENABLE, T&> operator*();
		const T& operator*() const;

		template<bool ENABLE = !CONST>
		enable_if_t<ENABLE, T*> operator->();
		const T* operator->() const;

		bool operator==(const LinkedListIterator<T, CONST>&) const;
		bool operator!=(const LinkedListIterator<T, CONST>&) const;
		operator bool() const;

	private:
		LinkedListIterator(data_type*, bool);

	private:
		data_type* m_current = nullptr;
		bool m_past_end = false;

		friend class LinkedList<T>;
		friend class LinkedListIterator<T, !CONST>;
	};

	template<typename T>
	LinkedList<T>& LinkedList<T>::operator=(const LinkedList<T>& other) requires is_copy_constructible_v<T>
	{
		clear();
		for (const T& elem : other)
			MUST(push_back(elem));
		return *this;
	}

	template<typename T>
	LinkedList<T>& LinkedList<T>::operator=(LinkedList<T>&& other)
	{
		clear();
		m_data = other.m_data;
		m_last = other.m_last;
		m_size = other.m_size;
		other.m_data = nullptr;
		other.m_last = nullptr;
		other.m_size = 0;
		return *this;
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::push_back(const T& value)
	{
		return push_back(move(T(value)));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::push_back(T&& value)
	{
		return insert(end(), move(value));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::insert(iterator iter, const T& value)
	{
		return insert(iter, move(T(value)));
	}

	template<typename T>
	ErrorOr<void> LinkedList<T>::insert(iterator iter, T&& value)
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
	ErrorOr<void> LinkedList<T>::emplace_back(Args&&... args)
	{
		return emplace(end(), forward<Args>(args)...);
	}

	template<typename T>
	template<typename... Args>
	ErrorOr<void> LinkedList<T>::emplace(iterator iter, Args&&... args)
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
		remove(iterator(m_last, false));
	}

	template<typename T>
	LinkedList<T>::iterator LinkedList<T>::remove(iterator iter)
	{
		ASSERT(!empty() && iter);
		Node* node = iter.m_current;
		Node* prev = node->prev;
		Node* next = node->next;
		node->value.~T();
		BAN::deallocator(node);
		(prev ? prev->next : m_data) = next;
		(next ? next->prev : m_last) = prev;
		m_size--;
		return next ? iterator(next, false) : iterator(m_last, true);
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
		ASSERT(!empty());
		return *const_iterator(m_last, false);
	}

	template<typename T>
	T& LinkedList<T>::back()
	{
		ASSERT(!empty());
		return *iterator(m_last, false);
	}

	template<typename T>
	const T& LinkedList<T>::front() const
	{
		ASSERT(!empty());
		return *const_iterator(m_data, false);
	}

	template<typename T>
	T& LinkedList<T>::front()
	{
		ASSERT(!empty());
		return *iterator(m_data, false);
	}

	template<typename T>
	bool LinkedList<T>::contains(const T& value) const
	{
		if (empty()) return false;
		for (Node* node = m_data;; node = node->next)
		{
			if (node->value == value)
				return true;
			if (node == m_last)
				return false;
		}
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
			return Error::from_errno(ENOMEM);
		return node;
	}

	template<typename T, bool CONST>
	template<bool C>
	LinkedListIterator<T, CONST>::LinkedListIterator(const LinkedListIterator<T, C>& other, enable_if_t<C == CONST || !C>*)
		: m_current(other.m_current)
		, m_past_end(other.m_past_end)
	{
	}

	template<typename T, bool CONST>
	LinkedListIterator<T, CONST>::LinkedListIterator(data_type* node, bool past_end)
		: m_current(node)
		, m_past_end(past_end)
	{
	}

	template<typename T, bool CONST>
	LinkedListIterator<T, CONST>& LinkedListIterator<T, CONST>::operator++()
	{
		ASSERT(m_current);
		ASSERT(m_current->next || !m_past_end);
		if (m_current->next)
			m_current = m_current->next;
		else
			m_past_end = true;
		return *this;
	}

	template<typename T, bool CONST>
	LinkedListIterator<T, CONST>& LinkedListIterator<T, CONST>::operator--()
	{
		ASSERT(m_current);
		ASSERT(m_current->prev || m_past_end);
		if (m_past_end)
			m_past_end = false;
		else
			m_current = m_current->prev;
		return *this;
	}

	template<typename T, bool CONST>
	LinkedListIterator<T, CONST> LinkedListIterator<T, CONST>::operator++(int)
	{
		auto temp = *this;
		++(*this);
		return temp;
	}

	template<typename T, bool CONST>
	LinkedListIterator<T, CONST> LinkedListIterator<T, CONST>::operator--(int)
	{
		auto temp = *this;
		--(*this);
		return temp;
	}

	template<typename T, bool CONST>
	template<bool ENABLE>
	enable_if_t<ENABLE, T&> LinkedListIterator<T, CONST>::operator*()
	{
		ASSERT(m_current);
		return m_current->value;
	}

	template<typename T, bool CONST>
	const T& LinkedListIterator<T, CONST>::operator*() const
	{
		ASSERT(m_current);
		return m_current->value;
	}

	template<typename T, bool CONST>
	template<bool ENABLE>
	enable_if_t<ENABLE, T*> LinkedListIterator<T, CONST>::operator->()
	{
		ASSERT(m_current);
		return &m_current->value;
	}

	template<typename T, bool CONST>
	const T* LinkedListIterator<T, CONST>::operator->() const
	{
		ASSERT(m_current);
		return &m_current->value;
	}

	template<typename T, bool CONST>
	bool LinkedListIterator<T, CONST>::operator==(const LinkedListIterator<T, CONST>& other) const
	{
		if (m_current != other.m_current)
			return false;
		return m_past_end == other.m_past_end;
	}

	template<typename T, bool CONST>
	bool LinkedListIterator<T, CONST>::operator!=(const LinkedListIterator<T, CONST>& other) const
	{
		return !(*this == other);
	}

	template<typename T, bool CONST>
	LinkedListIterator<T, CONST>::operator bool() const
	{
		return m_current;
	}

}
