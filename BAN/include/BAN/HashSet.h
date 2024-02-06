#pragma once

#include <BAN/Errors.h>
#include <BAN/Hash.h>
#include <BAN/Iterators.h>
#include <BAN/LinkedList.h>
#include <BAN/Math.h>
#include <BAN/Move.h>
#include <BAN/Vector.h>

namespace BAN
{

	template<typename T, typename HASH = hash<T>>
	class HashSet
	{
	public:
		using value_type = T;
		using size_type = size_t;
		using iterator = IteratorDouble<T, Vector, LinkedList, HashSet>;
		using const_iterator = ConstIteratorDouble<T, Vector, LinkedList, HashSet>;

	public:
		HashSet() = default;
		HashSet(const HashSet&);
		HashSet(HashSet&&);

		HashSet& operator=(const HashSet&);
		HashSet& operator=(HashSet&&);

		ErrorOr<void> insert(const T&);
		ErrorOr<void> insert(T&&);
		void remove(const T&);
		void clear();

		ErrorOr<void> reserve(size_type);

		iterator begin() { return iterator(m_buckets.end(), m_buckets.begin()); }
		iterator end()   { return iterator(m_buckets.end(), m_buckets.end()); }
		const_iterator begin() const { return const_iterator(m_buckets.end(), m_buckets.begin()); }
		const_iterator end() const   { return const_iterator(m_buckets.end(), m_buckets.end()); }

		bool contains(const T&) const;

		size_type size() const;
		bool empty() const;

	private:
		ErrorOr<void> rebucket(size_type);
		LinkedList<T>& get_bucket(const T&);
		const LinkedList<T>& get_bucket(const T&) const;

	private:
		Vector<LinkedList<T>> m_buckets;
		size_type m_size = 0;
	};

	template<typename T, typename HASH>
	HashSet<T, HASH>::HashSet(const HashSet& other)
		: m_buckets(other.m_buckets)
		, m_size(other.m_size)
	{
	}

	template<typename T, typename HASH>
	HashSet<T, HASH>::HashSet(HashSet&& other)
		: m_buckets(move(other.m_buckets))
		, m_size(other.m_size)
	{
		other.clear();
	}

	template<typename T, typename HASH>
	HashSet<T, HASH>& HashSet<T, HASH>::operator=(const HashSet& other)
	{
		clear();
		m_buckets = other.m_buckets;
		m_size = other.m_size;
		return *this;
	}

	template<typename T, typename HASH>
	HashSet<T, HASH>& HashSet<T, HASH>::operator=(HashSet&& other)
	{
		clear();
		m_buckets = move(other.m_buckets);
		m_size = other.m_size;
		other.clear();
		return *this;
	}

	template<typename T, typename HASH>
	ErrorOr<void> HashSet<T, HASH>::insert(const T& key)
	{
		return insert(move(T(key)));
	}

	template<typename T, typename HASH>
	ErrorOr<void> HashSet<T, HASH>::insert(T&& key)
	{
		if (!empty() && get_bucket(key).contains(key))
			return {};

		TRY(rebucket(m_size + 1));
		TRY(get_bucket(key).push_back(move(key)));
		m_size++;
		return {};
	}

	template<typename T, typename HASH>
	void HashSet<T, HASH>::remove(const T& key)
	{
		if (empty()) return;
		auto& bucket = get_bucket(key);
		for (auto it = bucket.begin(); it != bucket.end(); it++)
		{
			if (*it == key)
			{
				bucket.remove(it);
				m_size--;
				break;
			}
		}
	}

	template<typename T, typename HASH>
	void HashSet<T, HASH>::clear()
	{
		m_buckets.clear();
		m_size = 0;
	}

	template<typename T, typename HASH>
	ErrorOr<void> HashSet<T, HASH>::reserve(size_type size)
	{
		TRY(rebucket(size));
		return {};
	}

	template<typename T, typename HASH>
	bool HashSet<T, HASH>::contains(const T& key) const
	{
		if (empty()) return false;
		return get_bucket(key).contains(key);
	}

	template<typename T, typename HASH>
	typename HashSet<T, HASH>::size_type HashSet<T, HASH>::size() const
	{
		return m_size;
	}

	template<typename T, typename HASH>
	bool HashSet<T, HASH>::empty() const
	{
		return m_size == 0;
	}

	template<typename T, typename HASH>
	ErrorOr<void> HashSet<T, HASH>::rebucket(size_type bucket_count)
	{
		if (m_buckets.size() >= bucket_count)
			return {};

		size_type new_bucket_count = Math::max<size_type>(bucket_count, m_buckets.size() * 2);
		Vector<LinkedList<T>> new_buckets;
		if (new_buckets.resize(new_bucket_count).is_error())
			return Error::from_errno(ENOMEM);

		for (auto& bucket : m_buckets)
		{
			for (auto it = bucket.begin(); it != bucket.end();)
			{
				size_type new_bucket_index = HASH()(*it) % new_buckets.size();
				it = bucket.move_element_to_other_linked_list(new_buckets[new_bucket_index], new_buckets[new_bucket_index].end(), it);
			}
		}

		m_buckets = move(new_buckets);
		return {};
	}

	template<typename T, typename HASH>
	LinkedList<T>& HashSet<T, HASH>::get_bucket(const T& key)
	{
		ASSERT(!m_buckets.empty());
		size_type index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

	template<typename T, typename HASH>
	const LinkedList<T>& HashSet<T, HASH>::get_bucket(const T& key) const
	{
		ASSERT(!m_buckets.empty());
		size_type index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

}
