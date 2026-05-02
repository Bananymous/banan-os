#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>

#include <kernel/Memory/Types.h>

namespace Kernel
{

	class ByteRingBuffer
	{
	public:
		static BAN::ErrorOr<BAN::UniqPtr<ByteRingBuffer>> create(size_t size);
		~ByteRingBuffer();

		void push(BAN::ConstByteSpan data)
		{
			ASSERT(data.size() + m_size <= m_capacity);
			uint8_t* buffer_head = reinterpret_cast<uint8_t*>(m_vaddr) + (m_tail + m_size) % m_capacity;
			memcpy(buffer_head, data.data(), data.size());
			m_size += data.size();
		}

		void pop(size_t size)
		{
			ASSERT(size <= m_size);
			m_tail = (m_tail + size) % m_capacity;
			m_size -= size;
		}

		void pop_back(size_t size)
		{
			ASSERT(size <= m_size);
			m_size -= size;
		}

		BAN::ConstByteSpan get_data() const
		{
			const uint8_t* base = reinterpret_cast<const uint8_t*>(m_vaddr);
			return { base + m_tail, m_size };
		}

		uint8_t front() const
		{
			ASSERT(!empty());
			return reinterpret_cast<const uint8_t*>(m_vaddr)[m_tail];
		}

		uint8_t back() const
		{
			ASSERT(!empty());
			return reinterpret_cast<const uint8_t*>(m_vaddr)[m_tail + m_size - 1];
		}

		bool empty() const { return m_size == 0; }
		bool full() const { return m_size == m_capacity; }
		size_t free() const { return m_capacity - m_size; }
		size_t size() const { return m_size; }
		size_t capacity() const { return m_capacity; }

	private:
		ByteRingBuffer(size_t capacity)
			: m_capacity(capacity)
		{ }

	private:
		size_t m_size { 0 };
		size_t m_tail { 0 };
		const size_t m_capacity;

		vaddr_t m_vaddr { 0 };
	};

}
