#include <LibDEFLATE/Decompressor.h>
#include <LibDEFLATE/Utils.h>

namespace LibDEFLATE
{

	union ZLibHeader
	{
		struct
		{
			uint8_t cm : 4;
			uint8_t cinfo : 4;
			uint8_t fcheck : 5;
			uint8_t fdict : 1;
			uint8_t flevel : 2;
		};
		struct
		{
			uint8_t raw1;
			uint8_t raw2;
		};
	};

	BAN::ErrorOr<uint16_t> Decompressor::read_symbol(const HuffmanTree& tree)
	{
		const uint8_t instant_bits = tree.instant_bits();

		uint16_t code = reverse_bits(TRY(m_stream.peek_bits(instant_bits)), instant_bits);
		if (auto symbol = tree.get_symbol_instant(code); symbol.has_value())
		{
			MUST(m_stream.take_bits(symbol->len));
			return symbol->symbol;
		}

		MUST(m_stream.take_bits(instant_bits));

		uint8_t len = instant_bits;
		while (len < tree.max_bits())
		{
			code = (code << 1) | TRY(m_stream.take_bits(1));
			len++;
			if (auto symbol = tree.get_symbol(code, len); symbol.has_value())
				return symbol.value();
		}

		return BAN::Error::from_errno(EINVAL);
	}

	BAN::ErrorOr<void> Decompressor::inflate_block(const HuffmanTree& length_tree, const HuffmanTree& distance_tree)
	{
		uint16_t symbol;
		while ((symbol = TRY(read_symbol(length_tree))) != 256)
		{
			if (symbol < 256)
			{
				TRY(m_output.push_back(symbol));
				continue;
			}

			constexpr uint16_t length_base[] {
				3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
			};
			constexpr uint8_t length_extra_bits[] {
				0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
			};

			constexpr uint16_t distance_base[] {
				1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
			};
			constexpr uint8_t distance_extra_bits[] {
				0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
			};

			if (symbol > 285)
				return BAN::Error::from_errno(EINVAL);
			symbol -= 257;

			const uint16_t length = length_base[symbol] + TRY(m_stream.take_bits(length_extra_bits[symbol]));

			uint16_t distance_code;
			if (distance_tree.empty())
				distance_code = reverse_bits(TRY(m_stream.take_bits(5)), 5);
			else
				distance_code = TRY(read_symbol(distance_tree));
			if (distance_code > 29)
				return BAN::Error::from_errno(EINVAL);

			const uint16_t distance = distance_base[distance_code] + TRY(m_stream.take_bits(distance_extra_bits[distance_code]));

			const size_t orig_size = m_output.size();
			const size_t offset = orig_size - distance;
			TRY(m_output.resize(orig_size + length));
			for (size_t i = 0; i < length; i++)
				m_output[orig_size + i] = m_output[offset + i];
		}

		return {};
	}

	BAN::ErrorOr<void> Decompressor::handle_header()
	{
		switch (m_type)
		{
			case StreamType::Raw:
				return {};
			case StreamType::Zlib:
			{
				ZLibHeader header;
				header.raw1 = TRY(m_stream.take_bits(8));
				header.raw2 = TRY(m_stream.take_bits(8));

				if (((header.raw1 << 8) | header.raw2) % 31)
				{
					dwarnln("zlib header checksum failed");
					return BAN::Error::from_errno(EINVAL);
				}

				if (header.cm != 8)
				{
					dwarnln("zlib does not use DEFLATE");
					return BAN::Error::from_errno(EINVAL);
				}

				if (header.fdict)
				{
					TRY(m_stream.take_bits(16));
					TRY(m_stream.take_bits(16));
				}

				return {};
			}
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> Decompressor::handle_footer()
	{
		switch (m_type)
		{
			case StreamType::Raw:
				return {};
			case StreamType::Zlib:
			{
				m_stream.skip_to_byte_boundary();

				uint32_t adler32 = 0;
				for (size_t i = 0; i < 4; i++)
					adler32 = (adler32 << 8) | TRY(m_stream.take_bits(8));

				if (adler32 != calculate_adler32(m_output.span()))
				{
					dwarnln("zlib final adler32 checksum failed");
					return BAN::Error::from_errno(EINVAL);
				}

				return {};
			}
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> Decompressor::decompress_type0()
	{
		m_stream.skip_to_byte_boundary();
		const uint16_t len = TRY(m_stream.take_bits(16));
		const uint16_t nlen = TRY(m_stream.take_bits(16));
		if (len != 0xFFFF - nlen)
			return BAN::Error::from_errno(EINVAL);

		const size_t orig_size = m_output.size();
		TRY(m_output.resize(orig_size + len));
		TRY(m_stream.take_byte_aligned(&m_output[orig_size], len));

		return {};
	}

	BAN::ErrorOr<void> Decompressor::decompress_type1()
	{
		if (!m_fixed_tree.has_value())
			m_fixed_tree = TRY(HuffmanTree::fixed_tree());
		TRY(inflate_block(m_fixed_tree.value(), {}));
		return {};
	}

	BAN::ErrorOr<void> Decompressor::decompress_type2()
	{
		constexpr uint8_t code_length_order[] {
			16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
		};

		const uint16_t hlit  = TRY(m_stream.take_bits(5)) + 257;
		const uint8_t  hdist = TRY(m_stream.take_bits(5)) + 1;
		const uint8_t  hclen = TRY(m_stream.take_bits(4)) + 4;

		uint8_t code_lengths[19] {};
		for (size_t i = 0; i < hclen; i++)
			code_lengths[code_length_order[i]] = TRY(m_stream.take_bits(3));
		const auto code_length_tree = TRY(HuffmanTree::create({ code_lengths, 19 }));

		uint8_t bit_lengths[286 + 32] {};
		size_t bit_lengths_len = 0;

		uint16_t last_symbol = 0;
		while (bit_lengths_len < hlit + hdist)
		{
			uint16_t symbol = TRY(read_symbol(code_length_tree));
			if (symbol > 18)
				return BAN::Error::from_errno(EINVAL);

			uint8_t count;
			if (symbol <= 15)
			{
				count = 1;
			}
			else if (symbol == 16)
			{
				symbol = last_symbol;
				count = TRY(m_stream.take_bits(2)) + 3;
			}
			else if (symbol == 17)
			{
				symbol = 0;
				count = TRY(m_stream.take_bits(3)) + 3;
			}
			else
			{
				symbol = 0;
				count = TRY(m_stream.take_bits(7)) + 11;
			}

			ASSERT(bit_lengths_len + count <= hlit + hdist);

			for (uint8_t i = 0; i < count; i++)
				bit_lengths[bit_lengths_len++] = symbol;
			last_symbol = symbol;
		}

		TRY(inflate_block(
			TRY(HuffmanTree::create({ bit_lengths,        hlit  })),
			TRY(HuffmanTree::create({ bit_lengths + hlit, hdist }))
		));

		return {};
	}

	BAN::ErrorOr<BAN::Vector<uint8_t>> Decompressor::decompress()
	{
		TRY(handle_header());

		bool bfinal = false;
		while (!bfinal)
		{
			bfinal = TRY(m_stream.take_bits(1));
			switch (TRY(m_stream.take_bits(2)))
			{
				case 0b00:
					TRY(decompress_type0());
					break;
				case 0b01:
					TRY(decompress_type1());
					break;
				case 0b10:
					TRY(decompress_type2());
					break;
				default:
					return BAN::Error::from_errno(EINVAL);
			}
		}

		TRY(handle_footer());

		return BAN::move(m_output);
	}

}
