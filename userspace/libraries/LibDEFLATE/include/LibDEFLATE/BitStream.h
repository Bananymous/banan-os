#pragma once

#include <BAN/Vector.h>
#include <BAN/ByteSpan.h>

namespace LibDEFLATE
{

	class BitInputStream
	{
	public:
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

		BAN::ErrorOr<void> take_byte_aligned(uint8_t* output, size_t bytes)
		{
			ASSERT(m_bit_buffer % 8 == 0);

			while (m_bit_buffer_len && bytes)
			{
				*output++ = m_bit_buffer;
				m_bit_buffer >>= 8;
				m_bit_buffer_len -= 8;
				bytes--;
			}

			if (bytes > m_data.size())
				return BAN::Error::from_errno(EINVAL);
			memcpy(output, m_data.data(), bytes);
			m_data = m_data.slice(bytes);

			return {};
		}

		void skip_to_byte_boundary()
		{
			const size_t bits_to_remove = m_bit_buffer_len % 8;
			m_bit_buffer    >>= bits_to_remove;
			m_bit_buffer_len -= bits_to_remove;
		}

	private:
		BAN::ConstByteSpan m_data;
		uint32_t m_bit_buffer { 0 };
		uint8_t m_bit_buffer_len { 0 };
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
