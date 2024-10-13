#pragma once

#include <BAN/Hash.h>
#include <BAN/LinkedList.h>
#include <BAN/Vector.h>

namespace BAN
{

	template<typename Key, typename T, typename HASH = BAN::hash<Key>>
	class HashMap
	{
	public:
		struct Entry
		{
			template<typename... Args>
			Entry(const Key& key, Args&&... args) requires is_constructible_v<T, Args...>
				: key(key)
				, value(forward<Args>(args)...)
			{}

			Key key;
			T value;
		};

	public:
		using size_type = size_t;
		using key_type = Key;
		using value_type = T;
		using iterator = IteratorDouble<Entry, Vector, LinkedList, HashMap>;
		using const_iterator = ConstIteratorDouble<Entry, Vector, LinkedList, HashMap>;

	public:
		HashMap() = default;
		HashMap(const HashMap<Key, T, HASH>&);
		HashMap(HashMap<Key, T, HASH>&&);
		~HashMap();

		HashMap<Key, T, HASH>& operator=(const HashMap<Key, T, HASH>&);
		HashMap<Key, T, HASH>& operator=(HashMap<Key, T, HASH>&&);

		ErrorOr<void> insert(const Key& key, const T& value)           { return emplace(key, value); }
		ErrorOr<void> insert(const Key& key, T&& value)                { return emplace(key, move(value)); }
		ErrorOr<void> insert(Key&& key, const T& value)                { return emplace(move(key), value); }
		ErrorOr<void> insert(Key&& key, T&& value)                     { return emplace(move(key), move(value)); }

		ErrorOr<void> insert_or_assign(const Key& key, const T& value) { return emplace_or_assign(key, value); }
		ErrorOr<void> insert_or_assign(const Key& key, T&& value)      { return emplace_or_assign(key, move(value)); }
		ErrorOr<void> insert_or_assign(Key&& key, const T& value)      { return emplace_or_assign(move(key), value); }
		ErrorOr<void> insert_or_assign(Key&& key, T&& value)           { return emplace_or_assign(move(key), move(value)); }

		template<typename... Args>
		ErrorOr<void> emplace(const Key& key, Args&&... args) requires is_constructible_v<T, Args...>
		{ return emplace(Key(key), forward<Args>(args)...); }
		template<typename... Args>
		ErrorOr<void> emplace(Key&&, Args&&...) requires is_constructible_v<T, Args...>;

		template<typename... Args>
		ErrorOr<void> emplace_or_assign(const Key& key, Args&&... args) requires is_constructible_v<T, Args...>
		{ return emplace_or_assign(Key(key), forward<Args>(args)...); }
		template<typename... Args>
		ErrorOr<void> emplace_or_assign(Key&&, Args&&...) requires is_constructible_v<T, Args...>;

		iterator begin() { return iterator(m_buckets.end(), m_buckets.begin()); }
		iterator end()   { return iterator(m_buckets.end(), m_buckets.end()); }
		const_iterator begin() const { return const_iterator(m_buckets.end(), m_buckets.begin()); }
		const_iterator end() const   { return const_iterator(m_buckets.end(), m_buckets.end()); }

		ErrorOr<void> reserve(size_type);

		void remove(const Key&);
		void remove(iterator it);
		void clear();

		T& operator[](const Key&);
		const T& operator[](const Key&) const;

		iterator find(const Key& key);
		const_iterator find(const Key& key) const;
		bool contains(const Key&) const;

		bool empty() const;
		size_type size() const;

	private:
		ErrorOr<void> rebucket(size_type);
		LinkedList<Entry>& get_bucket(const Key&);
		const LinkedList<Entry>& get_bucket(const Key&) const;
		Vector<LinkedList<Entry>>::iterator get_bucket_iterator(const Key&);
		Vector<LinkedList<Entry>>::const_iterator get_bucket_iterator(const Key&) const;

	private:
		Vector<LinkedList<Entry>> m_buckets;
		size_type m_size = 0;

		friend iterator;
	};

	template<typename Key, typename T, typename HASH>
	HashMap<Key, T, HASH>::HashMap(const HashMap<Key, T, HASH>& other)
	{
		*this = other;
	}

	template<typename Key, typename T, typename HASH>
	HashMap<Key, T, HASH>::HashMap(HashMap<Key, T, HASH>&& other)
	{
		*this = move(other);
	}

	template<typename Key, typename T, typename HASH>
	HashMap<Key, T, HASH>::~HashMap()
	{
		clear();
	}

	template<typename Key, typename T, typename HASH>
	HashMap<Key, T, HASH>& HashMap<Key, T, HASH>::operator=(const HashMap<Key, T, HASH>& other)
	{
		clear();
		m_buckets = other.m_buckets;
		m_size = other.m_size;
		return *this;
	}

	template<typename Key, typename T, typename HASH>
	HashMap<Key, T, HASH>& HashMap<Key, T, HASH>::operator=(HashMap<Key, T, HASH>&& other)
	{
		clear();
		m_buckets = move(other.m_buckets);
		m_size = other.m_size;
		other.m_size = 0;
		return *this;
	}

	template<typename Key, typename T, typename HASH>
	template<typename... Args>
	ErrorOr<void> HashMap<Key, T, HASH>::emplace(Key&& key, Args&&... args) requires is_constructible_v<T, Args...>
	{
		ASSERT(!contains(key));
		TRY(rebucket(m_size + 1));
		auto& bucket = get_bucket(key);
		TRY(bucket.emplace_back(move(key), forward<Args>(args)...));
		m_size++;
		return {};
	}

	template<typename Key, typename T, typename HASH>
	template<typename... Args>
	ErrorOr<void> HashMap<Key, T, HASH>::emplace_or_assign(Key&& key, Args&&... args) requires is_constructible_v<T, Args...>
	{
		if (empty())
			return emplace(move(key), forward<Args>(args)...);
		auto& bucket = get_bucket(key);
		for (Entry& entry : bucket)
			if (entry.key == key)
				return {};
		TRY(bucket.emplace_back(move(key), forward<Args>(args)...));
		m_size++;
		return {};
	}

	template<typename Key, typename T, typename HASH>
	ErrorOr<void> HashMap<Key, T, HASH>::reserve(size_type size)
	{
		TRY(rebucket(size));
		return {};
	}

	template<typename Key, typename T, typename HASH>
	void HashMap<Key, T, HASH>::remove(const Key& key)
	{
		auto it = find(key);
		if (it != end())
			remove(it);
	}

	template<typename Key, typename T, typename HASH>
	void HashMap<Key, T, HASH>::remove(iterator it)
	{
		it.outer_current()->remove(it.inner_current());
		m_size--;
	}

	template<typename Key, typename T, typename HASH>
	void HashMap<Key, T, HASH>::clear()
	{
		m_buckets.clear();
		m_size = 0;
	}

	template<typename Key, typename T, typename HASH>
	T& HashMap<Key, T, HASH>::operator[](const Key& key)
	{
		ASSERT(!empty());
		auto& bucket = get_bucket(key);
		for (Entry& entry : bucket)
			if (entry.key == key)
				return entry.value;
		ASSERT(false);
	}

	template<typename Key, typename T, typename HASH>
	const T& HashMap<Key, T, HASH>::operator[](const Key& key) const
	{
		ASSERT(!empty());
		const auto& bucket = get_bucket(key);
		for (const Entry& entry : bucket)
			if (entry.key == key)
				return entry.value;
		ASSERT(false);
	}

	template<typename Key, typename T, typename HASH>
	typename HashMap<Key, T, HASH>::iterator HashMap<Key, T, HASH>::find(const Key& key)
	{
		if (empty())
			return end();
		auto bucket_it = get_bucket_iterator(key);
		for (auto it = bucket_it->begin(); it != bucket_it->end(); it++)
			if (it->key == key)
				return iterator(m_buckets.end(), bucket_it, it);
		return end();
	}

	template<typename Key, typename T, typename HASH>
	typename HashMap<Key, T, HASH>::const_iterator HashMap<Key, T, HASH>::find(const Key& key) const
	{
		if (empty())
			return end();
		auto bucket_it = get_bucket_iterator(key);
		for (auto it = bucket_it->begin(); it != bucket_it->end(); it++)
			if (it->key == key)
				return const_iterator(m_buckets.end(), bucket_it, it);
		return end();
	}

	template<typename Key, typename T, typename HASH>
	bool HashMap<Key, T, HASH>::contains(const Key& key) const
	{
		return find(key) != end();
	}

	template<typename Key, typename T, typename HASH>
	bool HashMap<Key, T, HASH>::empty() const
	{
		return m_size == 0;
	}

	template<typename Key, typename T, typename HASH>
	typename HashMap<Key, T, HASH>::size_type HashMap<Key, T, HASH>::size() const
	{
		return m_size;
	}

	template<typename Key, typename T, typename HASH>
	ErrorOr<void> HashMap<Key, T, HASH>::rebucket(size_type bucket_count)
	{
		if (m_buckets.size() >= bucket_count)
			return {};

		size_type new_bucket_count = BAN::Math::max<size_type>(bucket_count, m_buckets.size() * 2);
		Vector<LinkedList<Entry>> new_buckets;
		TRY(new_buckets.resize(new_bucket_count));

		for (auto& bucket : m_buckets)
		{
			for (auto it = bucket.begin(); it != bucket.end();)
			{
				size_type new_bucket_index = HASH()(it->key) % new_buckets.size();
				it = bucket.move_element_to_other_linked_list(new_buckets[new_bucket_index], new_buckets[new_bucket_index].end(), it);
			}
		}

		m_buckets = move(new_buckets);
		return {};
	}

	template<typename Key, typename T, typename HASH>
	LinkedList<typename HashMap<Key, T, HASH>::Entry>& HashMap<Key, T, HASH>::get_bucket(const Key& key)
	{
		return *get_bucket_iterator(key);
	}

	template<typename Key, typename T, typename HASH>
	const LinkedList<typename HashMap<Key, T, HASH>::Entry>& HashMap<Key, T, HASH>::get_bucket(const Key& key) const
	{
		return *get_bucket_iterator(key);
	}

	template<typename Key, typename T, typename HASH>
	Vector<LinkedList<typename HashMap<Key, T, HASH>::Entry>>::iterator HashMap<Key, T, HASH>::get_bucket_iterator(const Key& key)
	{
		ASSERT(!m_buckets.empty());
		auto index = HASH()(key) % m_buckets.size();
		return next(m_buckets.begin(), index);
	}

	template<typename Key, typename T, typename HASH>
	Vector<LinkedList<typename HashMap<Key, T, HASH>::Entry>>::const_iterator HashMap<Key, T, HASH>::get_bucket_iterator(const Key& key) const
	{
		ASSERT(!m_buckets.empty());
		auto index = HASH()(key) % m_buckets.size();
		return next(m_buckets.begin(), index);
	}

}
