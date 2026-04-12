#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Vector.h>

#include <string.h>

namespace LibDEFLATE
{

	class BitInputStream
	{
	public:
		BitInputStream() = default;
		BitInputStream(BAN::ConstByteSpan data)
			: m_data(data)
		{ }

		BAN::ErrorOr<uint16_t> peek_bits(size_t count)
		{
			ASSERT(count <= 16);

			while (m_bit_buffer_len < count)
			{
				if (m_data.empty())
					return BAN::Error::from_errno(ENOBUFS);
				m_bit_buffer |= m_data[0] << m_bit_buffer_len;
				m_bit_buffer_len += 8;
				m_data = m_data.slice(1);
			}

			return m_bit_buffer & ((1 << count) - 1);
		}

		BAN::ErrorOr<uint16_t> take_bits(size_t count)
		{
			const uint16_t result = TRY(peek_bits(count));
			m_bit_buffer >>= count;
			m_bit_buffer_len -= count;
			return result;
		}

		BAN::ErrorOr<void> take_byte_aligned(BAN::ByteSpan output)
		{
			ASSERT(m_bit_buffer_len % 8 == 0);

			while (m_bit_buffer_len && !output.empty())
			{
				output[0] = m_bit_buffer;
				m_bit_buffer >>= 8;
				m_bit_buffer_len -= 8;
				output = output.slice(1);
			}

			if (m_data.size() < output.size())
				return BAN::Error::from_errno(ENOBUFS);

			memcpy(output.data(), m_data.data(), output.size());

			m_data = m_data.slice(output.size());

			return {};
		}

		void skip_to_byte_boundary()
		{
			const size_t bits_to_remove = m_bit_buffer_len % 8;
			m_bit_buffer    >>= bits_to_remove;
			m_bit_buffer_len -= bits_to_remove;
		}

		size_t available_bits() const
		{
			return unprocessed_bytes() * 8 + m_bit_buffer_len;
		}

		size_t available_bytes() const
		{
			return unprocessed_bytes() + m_bit_buffer_len / 8;
		}

		size_t unprocessed_bytes() const
		{
			return m_data.size();
		}

		void set_data(BAN::ConstByteSpan data)
		{
			m_data = data;
		}

		void drop_unprocessed_data()
		{
			m_data = {};
		}

	private:
		BAN::ConstByteSpan m_data;
		uint32_t m_bit_buffer { 0 };
		uint32_t m_bit_buffer_len { 0 };
	};

	class BitOutputStream
	{
	public:
		BAN::ErrorOr<void> write_bits(uint16_t value, size_t count)
		{
			ASSERT(m_bit_buffer_len < 8);
			ASSERT(count <= 16);

			const uint16_t mask = (1 << count) - 1;
			m_bit_buffer |= (value & mask) << m_bit_buffer_len;
			m_bit_buffer_len += count;

			while (m_bit_buffer_len >= 8)
			{
				TRY(m_data.push_back(m_bit_buffer));
				m_bit_buffer    >>= 8;
				m_bit_buffer_len -= 8;
			}

			return {};
		}

		BAN::ErrorOr<void> pad_to_byte_boundary()
		{
			ASSERT(m_bit_buffer_len < 8);
			if (m_bit_buffer_len == 0)
				return {};
			TRY(m_data.push_back(m_bit_buffer));
			m_bit_buffer = 0;
			m_bit_buffer_len = 0;
			return {};
		}

		BAN::Vector<uint8_t> take_buffer()
		{
			ASSERT(m_bit_buffer_len == 0);
			return BAN::move(m_data);
		}

	private:
		BAN::Vector<uint8_t> m_data;
		uint32_t m_bit_buffer { 0 };
		uint8_t m_bit_buffer_len { 0 };
	};

}
