#pragma once

#include <BAN/Errors.h>
#include <BAN/Hash.h>
#include <BAN/Math.h>
#include <BAN/Move.h>
#include <BAN/New.h>

namespace BAN
{

	template<typename HashSet, typename Bucket, typename T>
	class HashSetIterator
	{
	public:
		HashSetIterator() = default;

		const T& operator*() const
		{
			ASSERT(m_bucket);
			return *m_bucket->element();
		}

		const T* operator->() const
		{
			ASSERT(m_bucket);
			return m_bucket->element();
		}

		HashSetIterator& operator++()
		{
			ASSERT(m_bucket);
			m_bucket++;
			skip_to_valid_bucket();
			return *this;
		}
		HashSetIterator operator++(int)
		{
			auto temp = *this;
			++(*this);
			return temp;
		}

		bool operator==(HashSetIterator other) const
		{
			return m_bucket == other.m_bucket;
		}
		bool operator!=(HashSetIterator other) const
		{
			return m_bucket != other.m_bucket;
		}

	private:
		explicit HashSetIterator(Bucket* bucket)
			: m_bucket(bucket)
		{
			if (m_bucket != nullptr)
				skip_to_valid_bucket();
		}

		void skip_to_valid_bucket()
		{
			while (m_bucket->state != Bucket::USED && !m_bucket->end)
				m_bucket++;
			if (m_bucket->end)
				m_bucket = nullptr;
		}

	private:
		Bucket* m_bucket { nullptr };
		friend HashSet;
	};

	namespace detail
	{
		template<typename T, typename U, typename HASH, typename COMP>
		concept HashSetFindable = requires(const U& a, const T& b) { COMP()(a, b); HASH()(b); };
	}

	template<typename T, typename HASH = BAN::hash<T>, typename COMP = BAN::equal<T>>
	class HashSet
	{
	private:
		struct Bucket
		{
			static constexpr uint8_t UNUSED  = 0;
			static constexpr uint8_t USED    = 1;
			static constexpr uint8_t REMOVED = 2;

			alignas(T) uint8_t storage[sizeof(T)];
			hash_t hash;
			uint8_t state       : 2;
			uint8_t chain_start : 1;
			uint8_t end         : 1;

			T* element() { return reinterpret_cast<T*>(storage); }
			const T* element() const { return reinterpret_cast<const T*>(storage); }
		};

	public:
		using value_type     = T;
		using size_type      = size_t;
		using iterator       = HashSetIterator<HashSet,       Bucket,       T>;
		using const_iterator = HashSetIterator<HashSet, const Bucket, const T>;

	public:
		HashSet() = default;
		~HashSet() { clear(); }

		HashSet(const HashSet& other) { *this = other; }
		HashSet& operator=(const HashSet& other)
		{
			clear();

			MUST(reserve(other.size()));
			for (auto& bucket : other)
				MUST(insert(bucket));

			return *this;
		}

		HashSet(HashSet&& other) { *this = BAN::move(other); }
		HashSet& operator=(HashSet&& other)
		{
			clear();

			m_buckets  = other.m_buckets;
			m_capacity = other.m_capacity;
			m_size     = other.m_size;
			m_removed  = other.m_removed;

			other.m_buckets  = nullptr;
			other.m_capacity = 0;
			other.m_size     = 0;
			other.m_removed  = 0;

			return *this;
		}

		iterator begin()             { return iterator(m_buckets); }
		iterator end()               { return iterator(nullptr); }
		const_iterator begin() const { return const_iterator(m_buckets); }
		const_iterator end() const   { return const_iterator(nullptr); }

		ErrorOr<iterator> insert(const T& value)
		{
			return insert(T(value));
		}

		ErrorOr<iterator> insert(T&& value)
		{
			if (should_rehash_with_size(m_size + 1))
				TRY(rehash(m_size * 2));
			return insert_impl(BAN::move(value), HASH()(value));
		}

		template<detail::HashSetFindable<T, HASH, COMP> U>
		void remove(const U& value)
		{
			if (auto it = find(value); it != end())
				remove(it);
		}

		iterator remove(iterator it)
		{
			auto& bucket = *it.m_bucket;
			bucket.element()->~T();
			bucket.state = Bucket::REMOVED;
			m_size--;
			m_removed++;
			return iterator(&bucket);
		}

		template<detail::HashSetFindable<T, HASH, COMP> U>
		iterator find(const U& value)
		{
			return iterator(const_cast<Bucket*>(find_impl(value).m_bucket));
		}

		template<detail::HashSetFindable<T, HASH, COMP> U>
		const_iterator find(const U& value) const
		{
			return find_impl(value);
		}

		void clear()
		{
			if (m_buckets == nullptr)
				return;

			for (size_type i = 0; i < m_capacity; i++)
				if (m_buckets[i].state == Bucket::USED)
					m_buckets[i].element()->~T();

			BAN::deallocator(m_buckets);
			m_buckets  = nullptr;
			m_capacity = 0;
			m_size     = 0;
			m_removed  = 0;
		}

		ErrorOr<void> reserve(size_type size)
		{
			if (should_rehash_with_size(size))
				TRY(rehash(size * 2));
			return {};
		}

		template<detail::HashSetFindable<T, HASH, COMP> U>
		bool contains(const U& value) const
		{
			return find(value) != end();
		}

		size_type capacity() const
		{
			return m_capacity;
		}

		size_type size() const
		{
			return m_size;
		}

		bool empty() const
		{
			return m_size == 0;
		}

	private:
		ErrorOr<void> rehash(size_type new_capacity)
		{
			new_capacity = BAN::Math::max<size_t>(16, BAN::Math::max(new_capacity, m_size + 1));
			new_capacity = BAN::Math::round_up_to_power_of_two(new_capacity);

			void* new_buckets = BAN::allocator((new_capacity + 1) * sizeof(Bucket));
			if (new_buckets == nullptr)
				return BAN::Error::from_errno(ENOMEM);
			memset(new_buckets, 0, (new_capacity + 1) * sizeof(Bucket));

			Bucket* old_buckets = m_buckets;
			const size_type old_capacity = m_capacity;

			m_buckets  = static_cast<Bucket*>(new_buckets);
			m_capacity = new_capacity;
			m_size     = 0;
			m_removed  = 0;

			for (size_type i = 0; i < old_capacity; i++)
			{
				auto& old_bucket = old_buckets[i];
				if (old_bucket.state != Bucket::USED)
					continue;
				insert_impl(BAN::move(*old_bucket.element()), old_bucket.hash);
				old_bucket.element()->~T();
			}

			m_buckets[m_capacity].end = true;

			BAN::deallocator(old_buckets);

			return {};
		}

		template<detail::HashSetFindable<T, HASH, COMP> U>
		const_iterator find_impl(const U& value) const
		{
			if (m_capacity == 0)
				return end();

			bool first = true;
			const hash_t orig_hash = HASH()(value);
			for (auto hash = orig_hash;; hash = get_next_hash_in_chain(hash, orig_hash), first = false)
			{
				auto& bucket = m_buckets[hash & (m_capacity - 1)];
				if (bucket.state == Bucket::USED && bucket.hash == orig_hash && COMP()(*bucket.element(), value))
					return const_iterator(&bucket);
				if (bucket.state == Bucket::UNUSED)
					return end();
				if (!first && bucket.chain_start)
					return end();
			}
		}

		iterator insert_impl(T&& value, hash_t orig_hash)
		{
			ASSERT(!should_rehash_with_size(m_size + 1));

			Bucket* target = nullptr;

			bool first = true;
			for (auto hash = orig_hash;; hash = get_next_hash_in_chain(hash, orig_hash), first = false)
			{
				auto& bucket = m_buckets[hash & (m_capacity - 1)];

				if (!first)
					bucket.chain_start = false;

				if (bucket.state == Bucket::USED)
				{
					if (bucket.hash != orig_hash || !COMP()(*bucket.element(), value))
						continue;
					target = &bucket;
					break;
				}

				if (target == nullptr)
					target = &bucket;

				if (bucket.state == Bucket::UNUSED)
					break;
			}

			switch (target->state)
			{
				case Bucket::USED:
					target->element()->~T();
					break;
				case Bucket::REMOVED:
					m_removed--;
					[[fallthrough]];
				case Bucket::UNUSED:
					m_size++;
					break;
			}

			target->chain_start = first && target->state == Bucket::UNUSED;
			target->hash = orig_hash;
			target->state = Bucket::USED;

			new (target->element()) T(BAN::move(value));

			return iterator(target);
		}

		bool should_rehash_with_size(size_type size) const
		{
			if (m_capacity < 16)
				return true;
			if (size + m_removed > m_capacity / 4 * 3)
				return true;
			return false;
		}

		hash_t get_next_hash_in_chain(hash_t prev_hash, hash_t orig_hash) const
		{
			// TODO: does this even provide better performance than `return prev_hash + 1`
			//       when using "good" hash functions
			return prev_hash * 1103515245 + (orig_hash | 1);
		}

	private:
		Bucket* m_buckets { nullptr };
		size_type m_capacity { 0 };
		size_type m_size     { 0 };
		size_type m_removed  { 0 };
	};

}
