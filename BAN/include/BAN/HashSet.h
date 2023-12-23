#pragma once

#include <BAN/Errors.h>
#include <BAN/Hash.h>
#include <BAN/Iterators.h>
#include <BAN/Math.h>
#include <BAN/Move.h>
#include <BAN/Vector.h>

namespace BAN
{

	template<typename T, typename HASH = hash<T>, bool STABLE = true>
	class HashSet
	{
	public:
		using value_type = T;
		using size_type = size_t;
		using iterator = IteratorDouble<T, Vector, Vector, HashSet>;
		using const_iterator = ConstIteratorDouble<T, Vector, Vector, HashSet>;
		
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
		Vector<T>& get_bucket(const T&);
		const Vector<T>& get_bucket(const T&) const;

	private:
		Vector<Vector<T>> m_buckets;
		size_type m_size = 0;
	};

	template<typename T, typename HASH, bool STABLE>
	HashSet<T, HASH, STABLE>::HashSet(const HashSet& other)
		: m_buckets(other.m_buckets)
		, m_size(other.m_size)
	{
	}

	template<typename T, typename HASH, bool STABLE>
	HashSet<T, HASH, STABLE>::HashSet(HashSet&& other)
		: m_buckets(move(other.m_buckets))
		, m_size(other.m_size)
	{
		other.clear();
	}

	template<typename T, typename HASH, bool STABLE>
	HashSet<T, HASH, STABLE>& HashSet<T, HASH, STABLE>::operator=(const HashSet& other)
	{
		clear();
		m_buckets = other.m_buckets;
		m_size = other.m_size;
		return *this;
	}

	template<typename T, typename HASH, bool STABLE>
	HashSet<T, HASH, STABLE>& HashSet<T, HASH, STABLE>::operator=(HashSet&& other)
	{
		clear();
		m_buckets = move(other.m_buckets);
		m_size = other.m_size;
		other.clear();
		return *this;
	}

	template<typename T, typename HASH, bool STABLE>
	ErrorOr<void> HashSet<T, HASH, STABLE>::insert(const T& key)
	{
		return insert(move(T(key)));
	}

	template<typename T, typename HASH, bool STABLE>
	ErrorOr<void> HashSet<T, HASH, STABLE>::insert(T&& key)
	{
		if (!empty() && get_bucket(key).contains(key))
			return {};

		TRY(rebucket(m_size + 1));
		TRY(get_bucket(key).push_back(move(key)));
		m_size++;
		return {};
	}

	template<typename T, typename HASH, bool STABLE>
	void HashSet<T, HASH, STABLE>::remove(const T& key)
	{
		if (empty()) return;
		Vector<T>& bucket = get_bucket(key);
		for (size_type i = 0; i < bucket.size(); i++)
		{
			if (bucket[i] == key)
			{
				bucket.remove(i);
				m_size--;
				break;
			}	
		}
	}

	template<typename T, typename HASH, bool STABLE>
	void HashSet<T, HASH, STABLE>::clear()
	{
		m_buckets.clear();
		m_size = 0;
	}

	template<typename T, typename HASH, bool STABLE>
	ErrorOr<void> HashSet<T, HASH, STABLE>::reserve(size_type size)
	{
		TRY(rebucket(size));
		return {};
	}

	template<typename T, typename HASH, bool STABLE>
	bool HashSet<T, HASH, STABLE>::contains(const T& key) const
	{
		if (empty()) return false;
		return get_bucket(key).contains(key);
	}

	template<typename T, typename HASH, bool STABLE>
	typename HashSet<T, HASH, STABLE>::size_type HashSet<T, HASH, STABLE>::size() const
	{
		return m_size;
	}

	template<typename T, typename HASH, bool STABLE>
	bool HashSet<T, HASH, STABLE>::empty() const
	{
		return m_size == 0;
	}

	template<typename T, typename HASH, bool STABLE>
	ErrorOr<void> HashSet<T, HASH, STABLE>::rebucket(size_type bucket_count)
	{
		if (m_buckets.size() >= bucket_count)
			return {};

		size_type new_bucket_count = BAN::Math::max<size_type>(bucket_count, m_buckets.size() * 2);
		Vector<Vector<T>> new_buckets;
		if (new_buckets.resize(new_bucket_count).is_error())
			return Error::from_errno(ENOMEM);

		for (auto& bucket : m_buckets)
		{
			for (T& key : bucket)
			{
				size_type bucket_index = HASH()(key) % new_buckets.size();
				if constexpr(STABLE)
					TRY(new_buckets[bucket_index].push_back(key));
				else
					TRY(new_buckets[bucket_index].push_back(move(key)));
			}
		}

		m_buckets = move(new_buckets);
		return {};
	}

	template<typename T, typename HASH, bool STABLE>
	Vector<T>& HashSet<T, HASH, STABLE>::get_bucket(const T& key)
	{
		ASSERT(!m_buckets.empty());
		size_type index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

	template<typename T, typename HASH, bool STABLE>
	const Vector<T>& HashSet<T, HASH, STABLE>::get_bucket(const T& key) const
	{
		ASSERT(!m_buckets.empty());
		size_type index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

	// Unstable hash set moves values between container during rebucketing.
	// This means that if insertion to set fails, elements could be in invalid state
	// and that container is no longer usable. This is better if either way you are
	// going to stop using the hash set after insertion fails.
	template<typename T, typename HASH = BAN::hash<T>>
	using HashSetUnstable = HashSet<T, HASH, false>;

}