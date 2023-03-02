#pragma once

#include <BAN/Errors.h>
#include <BAN/Hash.h>
#include <BAN/Math.h>
#include <BAN/Move.h>
#include <BAN/Vector.h>

namespace BAN
{

	template<typename T, typename HASH>
	class HashSetIterator;

	template<typename T, typename HASH = hash<T>>
	class HashSet
	{
	public:
		using value_type = T;
		using size_type = hash_t;
		using const_iterator = HashSetIterator<T, HASH>;
		
	public:
		HashSet() = default;
		HashSet(const HashSet<T, HASH>&);
		HashSet(HashSet<T, HASH>&&);

		HashSet<T, HASH>& operator=(const HashSet<T, HASH>&);
		HashSet<T, HASH>& operator=(HashSet<T, HASH>&&);

		ErrorOr<void> insert(const T&);
		ErrorOr<void> insert(T&&);
		void remove(const T&);
		void clear();

		ErrorOr<void> reserve(size_type);

		const_iterator begin() const { return const_iterator(this, m_buckets.begin()); }
		const_iterator end() const   { return const_iterator(this, m_buckets.end()); }

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

		friend class HashSetIterator<T, HASH>;
	};

	template<typename T, typename HASH>
	class HashSetIterator
	{
	public:
		HashSetIterator() = default;
		HashSetIterator(const HashSetIterator<T, HASH>&);
		
		HashSetIterator<T, HASH>& operator++();
		HashSetIterator<T, HASH> operator++(int);

		const T& operator*() const;
		const T* operator->() const;

		bool operator==(const HashSetIterator<T, HASH>&) const;
		bool operator!=(const HashSetIterator<T, HASH>&) const;

		operator bool() const { return m_owner && m_current_bucket; }

	private:
		HashSetIterator(const HashSet<T, HASH>* owner, Vector<Vector<T>>::const_iterator bucket);
		void find_next();

	private:
		const HashSet<T, HASH>*				m_owner = nullptr;
		Vector<Vector<T>>::const_iterator	m_current_bucket;
		Vector<T>::const_iterator			m_current_key;

		friend class HashSet<T, HASH>;
	};



	template<typename T, typename HASH>
	HashSet<T, HASH>::HashSet(const HashSet<T, HASH>& other)
		: m_buckets(other.m_buckets)
		, m_size(other.m_size)
	{
	}

	template<typename T, typename HASH>
	HashSet<T, HASH>::HashSet(HashSet<T, HASH>&& other)
		: m_buckets(move(other.m_buckets))
		, m_size(other.m_size)
	{
		other.clear();
	}

	template<typename T, typename HASH>
	HashSet<T, HASH>& HashSet<T, HASH>::operator=(const HashSet<T, HASH>& other)
	{
		clear();
		m_buckets = other.m_buckets;
		m_size = other.m_size;
		return *this;
	}

	template<typename T, typename HASH>
	HashSet<T, HASH>& HashSet<T, HASH>::operator=(HashSet<T, HASH>&& other)
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

		size_type new_bucket_count = BAN::Math::max<size_type>(bucket_count, m_buckets.size() * 2);
		Vector<Vector<T>> new_buckets;
		if (new_buckets.resize(new_bucket_count).is_error())
			return Error::from_errno(ENOMEM);
		
		// NOTE: We have to copy the old keys to the new keys and not move
		//       since we might run out of memory half way through.
		for (Vector<T>& bucket : m_buckets)
		{
			for (T& key : bucket)
			{
				size_type bucket_index = HASH()(key) % new_buckets.size();
				if (new_buckets[bucket_index].push_back(key).is_error())
					return Error::from_errno(ENOMEM);
			}
		}

		m_buckets = move(new_buckets);
		return {};
	}

	template<typename T, typename HASH>
	Vector<T>& HashSet<T, HASH>::get_bucket(const T& key)
	{
		ASSERT(!m_buckets.empty());
		size_type index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}

	template<typename T, typename HASH>
	const Vector<T>& HashSet<T, HASH>::get_bucket(const T& key) const
	{
		ASSERT(!m_buckets.empty());
		size_type index = HASH()(key) % m_buckets.size();
		return m_buckets[index];
	}



	template<typename T, typename HASH>
	HashSetIterator<T, HASH>& HashSetIterator<T, HASH>::operator++()
	{
		ASSERT(*this);
		if (m_current_key == m_current_bucket->end())
			m_current_bucket++;
		else
			m_current_key++;
		find_next();
		return *this;
	}

	template<typename T, typename HASH>
	HashSetIterator<T, HASH> HashSetIterator<T, HASH>::operator++(int)
	{
		auto temp = *this;
		++(*this);
		return temp;
	}

	template<typename T, typename HASH>
	const T& HashSetIterator<T, HASH>::operator*() const
	{
		ASSERT(m_owner && m_current_bucket && m_current_key);
		return *m_current_key;
	}

	template<typename T, typename HASH>
	const T* HashSetIterator<T, HASH>::operator->() const
	{
		return &**this;
	}

	template<typename T, typename HASH>
	bool HashSetIterator<T, HASH>::operator==(const HashSetIterator<T, HASH>& other) const
	{
		if (!m_owner || m_owner != other.m_owner)
			return false;
		return m_current_bucket == other.m_current_bucket
			&& m_current_key    == other.m_current_key;
	}

	template<typename T, typename HASH>
	bool HashSetIterator<T, HASH>::operator!=(const HashSetIterator<T, HASH>& other) const
	{
		return !(*this == other);
	}

	template<typename T, typename HASH>
	HashSetIterator<T, HASH>::HashSetIterator(const HashSet<T, HASH>* owner, Vector<Vector<T>>::const_iterator bucket)
		: m_owner(owner)
		, m_current_bucket(bucket)
	{
		if (m_current_bucket != m_owner->m_buckets.end())
			m_current_key = m_current_bucket->begin();
		find_next();
	}

	template<typename T, typename HASH>
	void HashSetIterator<T, HASH>::find_next()
	{
		ASSERT(m_owner && m_current_bucket);
		while (m_current_bucket != m_owner->m_buckets.end())
		{
			if (m_current_key && m_current_key != m_current_bucket->end())
				return;
			m_current_bucket++;
			m_current_key = m_current_bucket->begin();
		}
		m_current_key = typename Vector<T>::const_iterator();
	}

}