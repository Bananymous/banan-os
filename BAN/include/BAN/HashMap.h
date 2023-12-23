#pragma once

#include <BAN/Hash.h>
#include <BAN/LinkedList.h>
#include <BAN/Vector.h>

namespace BAN
{

	template<typename Key, typename T, typename HASH = BAN::hash<Key>, bool STABLE = true>
	class HashMap
	{
	public:
		struct Entry
		{
			template<typename... Args>
			Entry(const Key& key, Args&&... args)
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
		HashMap(const HashMap<Key, T, HASH, STABLE>&);
		HashMap(HashMap<Key, T, HASH, STABLE>&&);
		~HashMap();

		HashMap<Key, T, HASH, STABLE>& operator=(const HashMap<Key, T, HASH, STABLE>&);
		HashMap<Key, T, HASH, STABLE>& operator=(HashMap<Key, T, HASH, STABLE>&&);

		ErrorOr<void> insert(const Key&, const T&);
		ErrorOr<void> insert(const Key&, T&&);
		template<typename... Args>
		ErrorOr<void> emplace(const Key&, Args&&...);

		iterator begin() { return iterator(m_buckets.end(), m_buckets.begin()); }
		iterator end()   { return iterator(m_buckets.end(), m_buckets.end()); }
		const_iterator begin() const { return const_iterator(m_buckets.end(), m_buckets.begin()); }
		const_iterator end() const   { return const_iterator(m_buckets.end(), m_buckets.end()); }

		ErrorOr<void> reserve(size_type);

		void remove(const Key&);
		void clear();

		T& operator[](const Key&);
		const T& operator[](const Key&) const;

		bool contains(const Key&) const;

		bool empty() const;
		size_type size() const;

	private:
		ErrorOr<void> rebucket(size_type);
		LinkedList<Entry>& get_bucket(const Key&);
		const LinkedList<Entry>& get_bucket(const Key&) const;
		
	private:
		Vector<LinkedList<Entry>> m_buckets;
		size_type m_size = 0;

		friend iterator;
	};

	template<typename Key, typename T, typename HASH, bool STABLE>
	HashMap<Key, T, HASH, STABLE>::HashMap(const HashMap<Key, T, HASH, STABLE>& other)
	{
		*this = other;
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	HashMap<Key, T, HASH, STABLE>::HashMap(HashMap<Key, T, HASH, STABLE>&& other)
	{
		*this = move(other);
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	HashMap<Key, T, HASH, STABLE>::~HashMap()
	{
		clear();
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	HashMap<Key, T, HASH, STABLE>& HashMap<Key, T, HASH, STABLE>::operator=(const HashMap<Key, T, HASH, STABLE>& other)
	{
		clear();
		m_buckets = other.m_buckets;
		m_size = other.m_size;
		return *this;
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	HashMap<Key, T, HASH, STABLE>& HashMap<Key, T, HASH, STABLE>::operator=(HashMap<Key, T, HASH, STABLE>&& other)
	{
		clear();
		m_buckets = move(other.m_buckets);
		m_size = other.m_size;
		other.m_size = 0;
		return *this;
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	ErrorOr<void> HashMap<Key, T, HASH, STABLE>::insert(const Key& key, const T& value)
	{
		return insert(key, move(T(value)));
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	ErrorOr<void> HashMap<Key, T, HASH, STABLE>::insert(const Key& key, T&& value)
	{
		return emplace(key, move(value));
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	template<typename... Args>
	ErrorOr<void> HashMap<Key, T, HASH, STABLE>::emplace(const Key& key, Args&&... args)
	{
		ASSERT(!contains(key));
		TRY(rebucket(m_size + 1));
		auto& bucket = get_bucket(key);
		auto result = bucket.emplace_back(key, forward<Args>(args)...);
		if (result.is_error())
			return Error::from_errno(ENOMEM);
		m_size++;
		return {};
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	ErrorOr<void> HashMap<Key, T, HASH, STABLE>::reserve(size_type size)
	{
		TRY(rebucket(size));
		return {};
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	void HashMap<Key, T, HASH, STABLE>::remove(const Key& key)
	{
		if (empty()) return;
		auto& bucket = get_bucket(key);
		for (auto it = bucket.begin(); it != bucket.end(); it++)
		{
			if (it->key == key)
			{
				bucket.remove(it);
				m_size--;
				return;
			}
		}
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	void HashMap<Key, T, HASH, STABLE>::clear()
	{
		m_buckets.clear();
		m_size = 0;
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	T& HashMap<Key, T, HASH, STABLE>::operator[](const Key& key)
	{
		ASSERT(!empty());
		auto& bucket = get_bucket(key);
		for (Entry& entry : bucket)
			if (entry.key == key)
				return entry.value;
		ASSERT(false);
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	const T& HashMap<Key, T, HASH, STABLE>::operator[](const Key& key) const
	{
		ASSERT(!empty());
		const auto& bucket = get_bucket(key);
		for (const Entry& entry : bucket)
			if (entry.key == key)
				return entry.value;
		ASSERT(false);
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	bool HashMap<Key, T, HASH, STABLE>::contains(const Key& key) const
	{
		if (empty()) return false;
		const auto& bucket = get_bucket(key);
		for (const Entry& entry : bucket)
			if (entry.key == key)
				return true;
		return false;
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	bool HashMap<Key, T, HASH, STABLE>::empty() const
	{
		return m_size == 0;
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	typename HashMap<Key, T, HASH, STABLE>::size_type HashMap<Key, T, HASH, STABLE>::size() const
	{
		return m_size;
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	ErrorOr<void> HashMap<Key, T, HASH, STABLE>::rebucket(size_type bucket_count)
	{
		if (m_buckets.size() >= bucket_count)
			return {};

		size_type new_bucket_count = BAN::Math::max<size_type>(bucket_count, m_buckets.size() * 2);
		Vector<LinkedList<Entry>> new_buckets;
		if (new_buckets.resize(new_bucket_count).is_error())
			return Error::from_errno(ENOMEM);

		for (auto& bucket : m_buckets)
		{
			for (Entry& entry : bucket)
			{
				size_type bucket_index = HASH()(entry.key) % new_buckets.size();
				if constexpr(STABLE)
					TRY(new_buckets[bucket_index].push_back(entry));
				else
					TRY(new_buckets[bucket_index].push_back(BAN::move(entry)));
			}
		}

		m_buckets = move(new_buckets);
		return {};
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	LinkedList<typename HashMap<Key, T, HASH, STABLE>::Entry>& HashMap<Key, T, HASH, STABLE>::get_bucket(const Key& key)
	{
		ASSERT(!m_buckets.empty());
		auto index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

	template<typename Key, typename T, typename HASH, bool STABLE>
	const LinkedList<typename HashMap<Key, T, HASH, STABLE>::Entry>& HashMap<Key, T, HASH, STABLE>::get_bucket(const Key& key) const
	{
		ASSERT(!m_buckets.empty());
		auto index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

	// Unstable hash map moves values between container during rebucketing.
	// This means that if insertion to map fails, elements could be in invalid state
	// and that container is no longer usable. This is better if either way you are
	// going to stop using the hash map after insertion fails.
	template<typename Key, typename T, typename HASH = BAN::hash<Key>>
	using HashMapUnstable = HashMap<Key, T, HASH, false>;

}
