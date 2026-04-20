#pragma once

#include <BAN/HashSet.h>

namespace BAN
{

	template<typename HashSetIt, typename HashMap, typename Entry>
	class HashMapIterator
	{
	public:
		HashMapIterator() = default;

		Entry& operator*()
		{
			return m_iterator.operator*();
		}
		const Entry& operator*() const
		{
			return m_iterator.operator*();
		}

		Entry* operator->()
		{
			return m_iterator.operator->();
		}
		const Entry* operator->() const
		{
			return m_iterator.operator->();
		}

		HashMapIterator& operator++()
		{
			++m_iterator;
			return *this;
		}
		HashMapIterator operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		bool operator==(HashMapIterator other) const
		{
			return m_iterator == other.m_iterator;
		}
		bool operator!=(HashMapIterator other) const
		{
			return m_iterator != other.m_iterator;
		}

	private:
		explicit HashMapIterator(HashSetIt it)
			: m_iterator(it)
		{ }

	private:
		HashSetIt m_iterator;
		friend HashMap;
	};

	template<typename Key, typename T, typename HASH = BAN::hash<Key>, typename COMP = BAN::equal<Key>>
	class HashMap
	{
	public:
		struct Entry
		{
			Key key;
			T value;
		};

		struct EntryHash
		{
			constexpr bool operator()(const Key& a)
			{
				return HASH()(a);
			}
			constexpr bool operator()(const Entry& a)
			{
				return HASH()(a.key);
			}
		};

		struct EntryComp
		{
			constexpr bool operator()(const Entry& a, const Key& b)
			{
				return COMP()(a.key, b);
			}
			constexpr bool operator()(const Entry& a, const Entry& b)
			{
				return COMP()(a.key, b.key);
			}
		};

	public:
		using size_type      = size_t;
		using key_type       = Key;
		using value_type     = T;
		using iterator       = HashMapIterator<typename HashSet<Entry, EntryHash, EntryComp>::iterator,       HashMap, Entry>;
		using const_iterator = HashMapIterator<typename HashSet<Entry, EntryHash, EntryComp>::const_iterator, HashMap, const Entry>;

	public:
		HashMap() = default;
		~HashMap() { clear(); }

		HashMap(const HashMap<Key, T, HASH>& other) { *this = other; }
		HashMap<Key, T, HASH>& operator=(const HashMap<Key, T, HASH>& other)
		{
			m_hash_set = other.m_hash_set;
			return *this;
		}

		HashMap(HashMap<Key, T, HASH>&& other) { *this = BAN::move(other); }
		HashMap<Key, T, HASH>& operator=(HashMap<Key, T, HASH>&& other)
		{
			m_hash_set = BAN::move(other.m_hash_set);
			return *this;
		}

		iterator begin()             { return iterator(m_hash_set.begin()); }
		iterator end()               { return iterator(m_hash_set.end()); }
		const_iterator begin() const { return const_iterator(m_hash_set.begin()); }
		const_iterator end() const   { return const_iterator(m_hash_set.end()); }

		ErrorOr<iterator> insert(const Key& key, const T& value)           { return emplace(key, value); }
		ErrorOr<iterator> insert(const Key& key, T&& value)                { return emplace(key, move(value)); }
		ErrorOr<iterator> insert(Key&& key, const T& value)                { return emplace(move(key), value); }
		ErrorOr<iterator> insert(Key&& key, T&& value)                     { return emplace(move(key), move(value)); }

		ErrorOr<iterator> insert_or_assign(const Key& key, const T& value) { return emplace_or_assign(key, value); }
		ErrorOr<iterator> insert_or_assign(const Key& key, T&& value)      { return emplace_or_assign(key, move(value)); }
		ErrorOr<iterator> insert_or_assign(Key&& key, const T& value)      { return emplace_or_assign(move(key), value); }
		ErrorOr<iterator> insert_or_assign(Key&& key, T&& value)           { return emplace_or_assign(move(key), move(value)); }

		template<typename... Args>
		ErrorOr<iterator> emplace(const Key& key, Args&&... args) requires is_constructible_v<T, Args...>
		{ return emplace(Key(key), BAN::forward<Args>(args)...); }
		template<typename... Args>
		ErrorOr<iterator> emplace(Key&& key, Args&&... args) requires is_constructible_v<T, Args...>
		{
			ASSERT(!contains(key));
			auto it = TRY(m_hash_set.insert(Entry { BAN::move(key), T(BAN::forward<Args>(args)...) }));
			return iterator(it);
		}

		template<typename... Args>
		ErrorOr<iterator> emplace_or_assign(const Key& key, Args&&... args) requires is_constructible_v<T, Args...>
		{ return emplace_or_assign(Key(key), BAN::forward<Args>(args)...); }
		template<typename... Args>
		ErrorOr<iterator> emplace_or_assign(Key&& key, Args&&... args) requires is_constructible_v<T, Args...>
		{
			if (auto it = m_hash_set.find(key); it != m_hash_set.end())
			{
				it->value = T(BAN::forward<Args>(args)...);
				return iterator(it);
			}

			auto it = TRY(m_hash_set.insert(Entry { BAN::move(key), T(BAN::forward<Args>(args)...) }));
			return iterator(it);
		}

		void remove(const Key& key)
		{
			if (auto it = find(key); it != end())
				remove(it);
		}

		iterator remove(iterator it)
		{
			return iterator(m_hash_set.remove(it.m_iterator));
		}

		template<typename U> requires requires(const Key& a, const U& b) { COMP()(a, b); HASH()(b); }
		iterator find(const U& key)
		{
			return iterator(m_hash_set.find(key));
		}

		template<typename U> requires requires(const Key& a, const U& b) { COMP()(a, b); HASH()(b); }
		const_iterator find(const U& key) const
		{
			return const_iterator(m_hash_set.find(key));
		}

		void clear()
		{
			m_hash_set.clear();
		}

		ErrorOr<void> reserve(size_type size)
		{
			return m_hash_set.reserve(size);
		}

		T& operator[](const Key& key)
		{
			return find(key)->value;
		}

		const T& operator[](const Key& key) const
		{
			return find(key)->value;
		}

		bool contains(const Key& key) const
		{
			return find(key) != end();
		}

		size_type capacity() const
		{
			return m_hash_set.capacity();
		}

		size_type size() const
		{
			return m_hash_set.size();
		}

		bool empty() const
		{
			return m_hash_set.empty();
		}

	private:
		HashSet<Entry, EntryHash, EntryComp> m_hash_set;
	};

}
