#include <LibDEFLATE/Decompressor.h>
#include <LibDEFLATE/Utils.h>
#include <BAN/ScopeGuard.h>

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

				m_stream_info.zlib = {
					.s1 = 1,
					.s2 = 0,
					.adler32 = 0,
				};

				return {};
			}
			case StreamType::GZip:
			{
				const uint8_t id1 = TRY(m_stream.take_bits(8));
				const uint8_t id2 = TRY(m_stream.take_bits(8));
				if (id1 != 0x1F || id2 != 0x8B)
				{
					dwarnln("gzip header invalid identification");
					return BAN::Error::from_errno(EINVAL);

				}

				const uint8_t cm = TRY(m_stream.take_bits(8));
				if (cm != 8)
				{
					dwarnln("gzip does not use DEFLATE");
					return BAN::Error::from_errno(EINVAL);
				}

				const uint8_t flg = TRY(m_stream.take_bits(8));

				TRY(m_stream.take_bits(16)); // mtime
				TRY(m_stream.take_bits(16));

				TRY(m_stream.take_bits(8)); // xfl

				TRY(m_stream.take_bits(8)); // os

				// extra fields
				if (flg & (1 << 2))
				{
					const uint16_t xlen = TRY(m_stream.take_bits(16));
					for (size_t i = 0; i < xlen; i++)
						TRY(m_stream.take_bits(8));
				}

				// file name
				if (flg & (1 << 3))
					while (TRY(m_stream.take_bits(8)) != '\0')
						continue;

				// file comment
				if (flg & (1 << 4))
					while (TRY(m_stream.take_bits(8)) != '\0')
						continue;

				// crc16
				// TODO: validate
				if (flg & (1 << 1))
					TRY(m_stream.take_bits(16));

				m_stream_info.gzip = {
					.crc32 = 0xFFFFFFFF,
					.isize = 0,
				};

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

				auto& zlib = m_stream_info.zlib;
				zlib.adler32 = (zlib.s2 << 16) | zlib.s1;

				if (adler32 != zlib.adler32)
				{
					dwarnln("zlib final adler32 checksum failed {8h} vs {8h}", adler32, zlib.adler32);
					return BAN::Error::from_errno(EINVAL);
				}

				return {};
			}
			case StreamType::GZip:
			{
				m_stream.skip_to_byte_boundary();

				auto& gzip = m_stream_info.gzip;
				gzip.crc32 = ~gzip.crc32;

				const uint32_t crc32 =
					static_cast<uint32_t>(TRY(m_stream.take_bits(16))) |
					static_cast<uint32_t>(TRY(m_stream.take_bits(16))) << 16;

				if (crc32 != gzip.crc32)
				{
					dwarnln("gzip final crc32 checksum failed {8h} vs {8h}", crc32, gzip.crc32);
					return BAN::Error::from_errno(EINVAL);
				}

				const uint32_t isize =
					static_cast<uint32_t>(TRY(m_stream.take_bits(16))) |
					static_cast<uint32_t>(TRY(m_stream.take_bits(16))) << 16;

				if (isize != gzip.isize)
				{
					dwarnln("gzip final isize does not match {} vs {}", isize, gzip.isize);
					return BAN::Error::from_errno(EINVAL);
				}

				return {};
			}
		}

		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> Decompressor::handle_dynamic_header()
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

		m_length_tree   = TRY(HuffmanTree::create({ bit_lengths,        hlit  }));
		m_distance_tree = TRY(HuffmanTree::create({ bit_lengths + hlit, hdist }));

		return {};
	}

	BAN::ErrorOr<void> Decompressor::handle_symbol()
	{
		uint16_t symbol = TRY(read_symbol(m_length_tree));
		if (symbol == 256)
		{
			m_state = State::BlockHeader;
			return {};
		}

		if (symbol < 256)
		{
			m_window[(m_window_tail + m_window_size) % total_window_size] = symbol;

			m_produced_bytes++;
			if (m_window_size < total_window_size)
				m_window_size++;
			else
				m_window_tail = (m_window_tail + 1) % total_window_size;

			return {};
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
		if (m_distance_tree.empty())
			distance_code = reverse_bits(TRY(m_stream.take_bits(5)), 5);
		else
			distance_code = TRY(read_symbol(m_distance_tree));
		if (distance_code > 29)
			return BAN::Error::from_errno(EINVAL);

		const uint16_t distance = distance_base[distance_code] + TRY(m_stream.take_bits(distance_extra_bits[distance_code]));
		if (distance > m_window_size)
			return BAN::Error::from_errno(EINVAL);

		const size_t offset = m_window_size - distance;
		for (size_t i = 0; i < length; i++)
			m_window[(m_window_tail + m_window_size + i) % total_window_size] = m_window[(m_window_tail + offset + i) % total_window_size];

		m_window_size += length;
		m_produced_bytes += length;
		if (m_window_size > total_window_size)
		{
			const size_t extra = m_window_size - total_window_size;
			m_window_tail = (m_window_tail + extra) % total_window_size;
			m_window_size = total_window_size;
		}

		return {};
	}

	void Decompressor::write_data_to_output(BAN::ByteSpan& output)
	{
		if (m_produced_bytes == 0)
			return;

		ASSERT(m_produced_bytes <= m_window_size);

		const size_t unwritten_tail = (m_window_tail + m_window_size - m_produced_bytes) % total_window_size;

		const size_t to_write = BAN::Math::min(output.size(), m_produced_bytes);

		const size_t before_wrap = BAN::Math::min(total_window_size - unwritten_tail, to_write);
		memcpy(output.data(), m_window.data() + unwritten_tail, before_wrap);
		if (const size_t after_wrap = to_write - before_wrap)
			memcpy(output.data() + before_wrap, m_window.data(), after_wrap);

		switch (m_type)
		{
			case StreamType::Raw:
				break;
			case StreamType::Zlib:
			{
				auto& zlib = m_stream_info.zlib;
				for (size_t i = 0; i < to_write; i++)
				{
					zlib.s1 = (zlib.s1 + output[i]) % 65521;
					zlib.s2 = (zlib.s2 + zlib.s1)   % 65521;
				}
				break;
			}
			case StreamType::GZip:
			{
				auto& gzip = m_stream_info.gzip;
				gzip.isize += to_write;
				for (size_t i = 0; i < to_write; i++)
				{
					gzip.crc32 ^= output[i];

					for (size_t j = 0; j < 8; j++) {
						if (gzip.crc32 & 1)
							gzip.crc32 = (gzip.crc32 >> 1) ^ 0xEDB88320;
						else
							gzip.crc32 >>= 1;
					}
				}
				break;
			}
		}

		m_produced_bytes -= to_write;
		output = output.slice(to_write);
	}

	BAN::ErrorOr<BAN::Vector<uint8_t>> Decompressor::decompress(BAN::ConstByteSpan input)
	{
		BAN::Vector<uint8_t> full_output;
		TRY(full_output.resize(2 * input.size()));

		size_t total_output_size { 0 };
		for (;;)
		{
			size_t input_consumed, output_produced;
			const auto status = TRY(decompress(input, input_consumed, full_output.span().slice(total_output_size), output_produced));
			input = input.slice(input_consumed);
			total_output_size += output_produced;

			switch (status)
			{
				case Status::Done:
					TRY(full_output.resize(total_output_size));
					(void)full_output.shrink_to_fit();
					return full_output;
				case Status::NeedMoreOutput:
					TRY(full_output.resize(full_output.size() * 2));
					break;
				case Status::NeedMoreInput:
					return BAN::Error::from_errno(EINVAL);
			}
		}
	}

	BAN::ErrorOr<BAN::Vector<uint8_t>> Decompressor::decompress(BAN::Span<const BAN::ConstByteSpan> input)
	{
		size_t total_input_size = 0;
		for (const auto& buffer : input)
			total_input_size += buffer.size();

		BAN::Vector<uint8_t> full_output;
		TRY(full_output.resize(2 * total_input_size));

		BAN::Vector<uint8_t> input_buffer;
		TRY(input_buffer.resize(BAN::Math::min<size_t>(32 * 1024, total_input_size)));

		size_t input_buffer_index = 0;
		size_t input_buffer_size = 0;

		const auto append_input_data =
			[&]() -> bool
			{
				bool did_append = false;
				while (!input.empty() && input_buffer_size < input_buffer.size())
				{
					if (input_buffer_index >= input[0].size())
					{
						input_buffer_index = 0;
						input = input.slice(1);
						continue;
					}

					const size_t to_copy = BAN::Math::min(input[0].size() - input_buffer_index, input_buffer.size() - input_buffer_size);
					memcpy(input_buffer.data() + input_buffer_size, input[0].data() + input_buffer_index, to_copy);
					input_buffer_size += to_copy;
					input_buffer_index += to_copy;
					did_append = true;
				}
				return did_append;
			};

		append_input_data();

		size_t total_output_size = 0;
		for (;;)
		{
			size_t input_consumed, output_produced;
			const auto status = TRY(decompress(
				input_buffer.span().slice(0, input_buffer_size),
				input_consumed,
				full_output.span().slice(total_output_size),
				output_produced
			));

			if (input_consumed)
			{
				memmove(input_buffer.data(), input_buffer.data() + input_consumed, input_buffer_size - input_consumed);
				input_buffer_size -= input_consumed;
			}

			total_output_size += output_produced;

			switch (status)
			{
				case Status::Done:
					TRY(full_output.resize(total_output_size));
					(void)full_output.shrink_to_fit();
					return full_output;
				case Status::NeedMoreOutput:
					TRY(full_output.resize(full_output.size() * 2));
					break;
				case Status::NeedMoreInput:
					if (!append_input_data())
						return BAN::Error::from_errno(EINVAL);
					break;
			}
		}
	}

	BAN::ErrorOr<Decompressor::Status> Decompressor::decompress(BAN::ConstByteSpan input, size_t& input_consumed, BAN::ByteSpan output, size_t& output_produced)
	{
		const size_t original_input_size = input.size();
		const size_t original_output_size = output.size();
		BAN::ScopeGuard _([&] {
			input_consumed = original_input_size - m_stream.unprocessed_bytes();
			output_produced = original_output_size - output.size();
			m_stream.drop_unprocessed_data();
		});

		m_stream.set_data(input);

		if (m_window.empty())
			TRY(m_window.resize(total_window_size));

		write_data_to_output(output);
		if (m_produced_bytes > 0)
			return Status::NeedMoreOutput;

		while (m_state != State::Done)
		{
			bool need_more_input = false;
			bool restore_saved_stream = false;

			const auto saved_stream = m_stream;

			switch (m_state)
			{
				case State::Done:
					ASSERT_NOT_REACHED();
				case State::StreamHeader:
				{
					if (auto ret = handle_header(); !ret.is_error())
						m_state = State::BlockHeader;
					else
					{
						if (ret.error().get_error_code() != ENOBUFS)
							return ret.release_error();
						need_more_input = true;
						restore_saved_stream = true;
					}
					break;
				}
				case State::StreamFooter:
				{
					if (auto ret = handle_footer(); !ret.is_error())
						m_state = State::Done;
					else
					{
						if (ret.error().get_error_code() != ENOBUFS)
							return ret.release_error();
						need_more_input = true;
						restore_saved_stream = true;
					}
					break;
				}
				case State::BlockHeader:
				{
					if (m_bfinal)
					{
						m_state = State::StreamFooter;
						break;
					}

					if (m_stream.available_bits() < 3)
					{
						need_more_input = true;
						break;
					}

					m_bfinal = MUST(m_stream.take_bits(1));
					switch (MUST(m_stream.take_bits(2)))
					{
						case 0b00:
							m_state = State::LiteralHeader;
							break;
						case 0b01:
							m_length_tree = TRY(HuffmanTree::fixed_tree());
							m_distance_tree = {};
							m_state = State::Symbol;
							break;
						case 0b10:
							m_state = State::DynamicHeader;
							break;
						default:
							return BAN::Error::from_errno(EINVAL);
					}

					break;
				}
				case State::LiteralHeader:
				{
					if (m_stream.available_bytes() < 4)
					{
						need_more_input = true;
						break;
					}

					m_stream.skip_to_byte_boundary();
					const uint16_t len = MUST(m_stream.take_bits(16));
					const uint16_t nlen = MUST(m_stream.take_bits(16));
					if (len != 0xFFFF - nlen)
						return BAN::Error::from_errno(EINVAL);

					m_raw_bytes_left = len;
					m_state = State::ReadRaw;
					break;
				}
				case State::DynamicHeader:
				{
					if (auto ret = handle_dynamic_header(); !ret.is_error())
						m_state = State::Symbol;
					else
					{
						if (ret.error().get_error_code() != ENOBUFS)
							return ret.release_error();
						need_more_input = true;
						restore_saved_stream = true;
					}
					break;
				}
				case State::ReadRaw:
				{
					const size_t window_head = (m_window_tail + m_window_size) % total_window_size;

					// FIXME: m_raw_bytes_left can be up to 64KB
					const size_t max_bytes_to_read = BAN::Math::min<size_t>(m_raw_bytes_left, total_window_size);

					const size_t can_read = BAN::Math::min(max_bytes_to_read, m_stream.available_bytes());
					const size_t before_wrap = BAN::Math::min(total_window_size - window_head, can_read);
					MUST(m_stream.take_byte_aligned(BAN::ByteSpan(m_window.span()).slice(window_head, before_wrap)));
					if (const size_t after_wrap = can_read - before_wrap)
						MUST(m_stream.take_byte_aligned(BAN::ByteSpan(m_window.span()).slice(0, after_wrap)));

					m_window_size += can_read;
					m_produced_bytes += can_read;
					if (m_window_size > total_window_size)
					{
						const size_t extra = m_window_size - total_window_size;
						m_window_tail = (m_window_tail + extra) % total_window_size;
						m_window_size = total_window_size;
					}

					m_raw_bytes_left -= can_read;

					if (m_raw_bytes_left == 0)
						m_state = State::BlockHeader;
					else if (m_stream.available_bytes() == 0)
						need_more_input = true;

					break;
				}
				case State::Symbol:
				{
					if (auto ret = handle_symbol(); ret.is_error())
					{
						if (ret.error().get_error_code() != ENOBUFS)
							return ret.release_error();
						need_more_input = true;
						restore_saved_stream = true;
					}
					break;
				}
			}

			if (need_more_input)
			{
				if (restore_saved_stream)
					m_stream = saved_stream;
				return Status::NeedMoreInput;
			}

			write_data_to_output(output);
			if (m_produced_bytes > 0)
				return Status::NeedMoreOutput;
		}

		return Status::Done;
	}

}
