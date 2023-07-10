#pragma once

#include <BAN/Hash.h>
#include <BAN/LinkedList.h>
#include <BAN/Vector.h>

namespace BAN
{

	template<typename Container>
	class HashMapIterator;

	template<typename Key, typename T, typename HASH = BAN::hash<Key>>
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
		using iterator = HashMapIterator<HashMap>;

	public:
		HashMap() = default;
		HashMap(const HashMap<Key, T, HASH>&);
		HashMap(HashMap<Key, T, HASH>&&);
		~HashMap();

		HashMap<Key, T, HASH>& operator=(const HashMap<Key, T, HASH>&);
		HashMap<Key, T, HASH>& operator=(HashMap<Key, T, HASH>&&);

		ErrorOr<void> insert(const Key&, const T&);
		ErrorOr<void> insert(const Key&, T&&);
		template<typename... Args>
		ErrorOr<void> emplace(const Key&, Args&&...);

		iterator begin() { return iterator(m_buckets, m_buckets.begin()); }
		iterator end()   { return iterator(m_buckets, m_buckets.end()); }

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

	template<typename Container>
	class HashMapIterator
	{
	public:
		using KeyValue = typename Container::Entry;

	public:
		HashMapIterator() = default;

		const KeyValue& operator*() const
		{
			ASSERT(*this);
			ASSERT(!at_end());
			return *m_entry_iterator;
		}
		KeyValue& operator*()
		{
			ASSERT(*this);
			ASSERT(!at_end());
			return *m_entry_iterator;
		}

		HashMapIterator& operator++()
		{
			ASSERT(*this);
			ASSERT(!at_end());
			++m_entry_iterator;
			while (m_entry_iterator == m_bucket_iterator->end())
			{
				++m_bucket_iterator;
				if (m_bucket_iterator == m_container.end())
				{
					m_entry_iterator = {};
					break;
				}
				m_entry_iterator = m_bucket_iterator->begin();
			}
			return *this;
		}

		HashMapIterator operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		bool operator==(const HashMapIterator& other) const
		{
			if (&m_container != &other.m_container)
				return false;
			if (m_bucket_iterator != other.m_bucket_iterator)
				return false;
			if (m_bucket_iterator == m_container.end())
				return true;
			if (m_entry_iterator && other.m_entry_iterator)
				return m_entry_iterator == other.m_entry_iterator;
			if (!m_entry_iterator && !other.m_entry_iterator)
				return true;
			return false;
		}
		bool operator!=(const HashMapIterator& other) const
		{
			return !(*this == other);
		}

		operator bool() const
		{
			return m_bucket_iterator && m_entry_iterator;
		}

	private:
		HashMapIterator(Vector<LinkedList<KeyValue>>& container, Vector<LinkedList<KeyValue>>::iterator current)
			: m_container(container)
			, m_bucket_iterator(current)
		{
			if (current != container.end())
				m_entry_iterator = current->begin();
		}

		bool at_end() const
		{
			return m_bucket_iterator == m_container.end();
		}

	private:
		Vector<LinkedList<KeyValue>>& m_container;
		Vector<LinkedList<KeyValue>>::iterator m_bucket_iterator;
		LinkedList<KeyValue>::iterator m_entry_iterator;

		friend Container;
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
	ErrorOr<void> HashMap<Key, T, HASH>::insert(const Key& key, const T& value)
	{
		return insert(key, move(T(value)));
	}

	template<typename Key, typename T, typename HASH>
	ErrorOr<void> HashMap<Key, T, HASH>::insert(const Key& key, T&& value)
	{
		return emplace(key, move(value));
	}

	template<typename Key, typename T, typename HASH>
	template<typename... Args>
	ErrorOr<void> HashMap<Key, T, HASH>::emplace(const Key& key, Args&&... args)
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

	template<typename Key, typename T, typename HASH>
	ErrorOr<void> HashMap<Key, T, HASH>::reserve(size_type size)
	{
		TRY(rebucket(size));
		return {};
	}

	template<typename Key, typename T, typename HASH>
	void HashMap<Key, T, HASH>::remove(const Key& key)
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
	bool HashMap<Key, T, HASH>::contains(const Key& key) const
	{
		if (empty()) return false;
		const auto& bucket = get_bucket(key);
		for (const Entry& entry : bucket)
			if (entry.key == key)
				return true;
		return false;
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
		if (new_buckets.resize(new_bucket_count).is_error())
			return Error::from_errno(ENOMEM);
		
		// NOTE: We have to copy the old entries to the new entries and not move
		//       since we might run out of memory half way through.
		for (auto& bucket : m_buckets)
		{
			for (Entry& entry : bucket)
			{
				size_type bucket_index = HASH()(entry.key) % new_buckets.size();
				if (new_buckets[bucket_index].push_back(entry).is_error())
					return Error::from_errno(ENOMEM);
			}
		}

		m_buckets = move(new_buckets);
		return {};
	}

	template<typename Key, typename T, typename HASH>
	LinkedList<typename HashMap<Key, T, HASH>::Entry>& HashMap<Key, T, HASH>::get_bucket(const Key& key)
	{
		ASSERT(!m_buckets.empty());
		auto index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

	template<typename Key, typename T, typename HASH>
	const LinkedList<typename HashMap<Key, T, HASH>::Entry>& HashMap<Key, T, HASH>::get_bucket(const Key& key) const
	{
		ASSERT(!m_buckets.empty());
		auto index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

}