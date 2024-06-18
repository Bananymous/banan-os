#include <BAN/Debug.h>
#include <BAN/Endianness.h>

#include <LibImage/PNG.h>

#include <ctype.h>

#define DEBUG_PNG 0

// PNG     https://www.w3.org/TR/png-3/
// ZLIB    https://www.rfc-editor.org/rfc/rfc1950
// DEFLATE https://www.rfc-editor.org/rfc/rfc1951

namespace LibImage
{

	template<BAN::integral T>
	struct PODNetworkEndian
	{
		T raw;
		constexpr operator T() const { return BAN::network_endian_to_host(raw); }
	};

	enum class ColourType : uint8_t
	{
		Greyscale = 0,
		Truecolour = 2,
		IndexedColour = 3,
		GreyscaleAlpha = 4,
		TruecolourAlpha = 6,
	};

	enum class CompressionMethod : uint8_t
	{
		Deflate = 0,
	};

	enum class FilterMethod : uint8_t
	{
		Adaptive = 0,
	};

	enum class FilterType : uint8_t
	{
		None = 0,
		Sub = 1,
		Up = 2,
		Average = 3,
		Paeth = 4,
	};

	enum class InterlaceMethod : uint8_t
	{
		NoInterlace = 0,
		Adam7 = 1,
	};

	struct IHDR
	{
		PODNetworkEndian<uint32_t> width;
		PODNetworkEndian<uint32_t> height;
		uint8_t                    bit_depth;
		ColourType                 colour_type;
		CompressionMethod          compression_method;
		FilterMethod               filter_method;
		InterlaceMethod            interlace_method;
	} __attribute__((packed));

	struct ZLibStream
	{
		uint8_t cm : 4;
		uint8_t cinfo : 4;
		uint8_t fcheck : 5;
		uint8_t fdict : 1;
		uint8_t flevel : 2;
	};

	struct PNGChunk
	{
		BAN::StringView name;
		BAN::ConstByteSpan data;
	};

	class BitBuffer
	{
	public:
		BitBuffer(BAN::Vector<BAN::ConstByteSpan> data)
			: m_data(data)
		{}

		BAN::ErrorOr<uint16_t> peek_bits(uint8_t count)
		{
			ASSERT(count <= 16);

			while (m_bit_buffer_len < count)
			{
				if (m_data.empty())
					return BAN::Error::from_errno(ENODATA);
				m_bit_buffer |= m_data[0][0] << m_bit_buffer_len;
				m_bit_buffer_len += 8;
				if (m_data[0].size() > 1)
					m_data[0] = m_data[0].slice(1);
				else
					m_data.remove(0);
			}

			return m_bit_buffer & ((1 << count) - 1);
		}

		void remove_bits(uint8_t count)
		{
			ASSERT(count <= 16);
			ASSERT(m_bit_buffer_len >= count);
			m_bit_buffer_len -= count;
			m_bit_buffer >>= count;
		}

		BAN::ErrorOr<uint16_t> get_bits(uint8_t count)
		{
			uint16_t result = TRY(peek_bits(count));
			remove_bits(count);
			return result;
		}

		void skip_to_byte_boundary()
		{
			m_bit_buffer >>= m_bit_buffer_len % 8;
			m_bit_buffer_len -= m_bit_buffer_len % 8;
		}

	private:
		BAN::Vector<BAN::ConstByteSpan> m_data;
		uint32_t m_bit_buffer { 0 };
		uint8_t m_bit_buffer_len { 0 };
	};

	constexpr uint16_t reverse_bits(uint16_t value, uint8_t count)
	{
		uint16_t reverse = 0;
		for (uint8_t bit = 0; bit < count; bit++)
			reverse |= ((value >> bit) & 1) << (count - bit - 1);
		return reverse;
	}

	class HuffmanTree
	{
	public:
		static constexpr uint8_t MAX_BITS = 15;

		struct Leaf
		{
			uint16_t code;
			uint8_t len;
		};

	public:
		HuffmanTree() = default;
		HuffmanTree(BAN::Vector<Leaf>&& leaves, uint8_t min_len, uint8_t max_len, uint8_t instant_max_bit)
			: m_leaves(BAN::move(leaves))
			, m_min_bits(min_len), m_max_bits(max_len)
			, m_instant_max_bit(instant_max_bit)
		{}

		uint8_t min_bits() const { return m_min_bits; }
		uint8_t max_bits() const { return m_max_bits; }
		uint8_t instant_max_bit() const { return m_instant_max_bit; }
		Leaf get_leaf(size_t index) const { return m_leaves[index]; }
		bool empty() const { return m_leaves.empty(); }

		static BAN::ErrorOr<HuffmanTree> create(const BAN::Vector<uint8_t>& bit_lengths)
		{
			uint16_t bl_count[MAX_BITS] {};
			for (uint8_t bl : bit_lengths)
				bl_count[bl]++;
			bl_count[0] = 0;

			uint8_t min_bits = MAX_BITS;
			uint8_t max_bits = 0;
			for (uint8_t bits = 0; bits <= MAX_BITS; bits++)
			{
				if (bit_lengths[bits] == 0)
					continue;
				min_bits = BAN::Math::min(min_bits, bits);
				max_bits = BAN::Math::max(max_bits, bits);
			}

			uint8_t instant_max_bit = BAN::Math::min<uint8_t>(10, max_bits);
			uint16_t instant_mask = (1 << instant_max_bit) - 1;

			uint16_t code = 0;
			uint16_t next_code[MAX_BITS + 1] {};
			for (uint8_t bits = 1; bits <= max_bits; bits++)
			{
				code = (code + bl_count[bits - 1]) << 1;
				next_code[bits] = code;
			}

			BAN::Vector<Leaf> leaves;
			TRY(leaves.resize(1 << max_bits));

			for (uint16_t n = 0; n < bit_lengths.size(); n++)
			{
				uint8_t bits = bit_lengths[n];
				if (bits == 0)
					continue;

				uint16_t canonical = next_code[bits];
				next_code[bits]++;

				uint16_t reversed = reverse_bits(canonical, bits);
				leaves[reversed] = Leaf { n, bits };

				if (bits <= instant_max_bit)
				{
					uint16_t step = 1 << bits;
					for (uint16_t spread = reversed + step; spread <= instant_mask; spread += step)
						leaves[spread] = Leaf { n, bits };
				}
			}

			return HuffmanTree(BAN::move(leaves), min_bits, max_bits, instant_max_bit);
		}

		static BAN::ErrorOr<HuffmanTree> fixed_tree()
		{
			BAN::Vector<uint8_t> bit_lengths;
			TRY(bit_lengths.resize(288));
			size_t i = 0;
			for (; i <= 143; i++) bit_lengths[i] = 8;
			for (; i <= 255; i++) bit_lengths[i] = 9;
			for (; i <= 279; i++) bit_lengths[i] = 7;
			for (; i <= 287; i++) bit_lengths[i] = 8;
			return TRY(HuffmanTree::create(bit_lengths));
		}

	private:
		BAN::Vector<Leaf> m_leaves;
		uint8_t m_min_bits        { 0 };
		uint8_t m_max_bits        { 0 };
		uint8_t m_instant_max_bit { 0 };
	};

	class DeflateDecoder
	{
	public:
		DeflateDecoder(BAN::Vector<BAN::ConstByteSpan> data)
			: m_buffer(BitBuffer(BAN::move(data)))
		{}

		BAN::ErrorOr<BAN::ByteSpan> decode_stream()
		{
			while (!TRY(decode_block()))
				continue;

			m_buffer.skip_to_byte_boundary();

			uint32_t checksum = 0;
			for (int i = 0; i < 4; i++)
				checksum = (checksum << 8) | TRY(m_buffer.get_bits(8));

			if (decoded_adler32() != checksum)
			{
				dwarnln_if(DEBUG_PNG, "decode checksum does not match");
				return BAN::Error::from_errno(EINVAL);
			}

			return BAN::ByteSpan(m_decoded.span());
		}

	private:
		uint32_t decoded_adler32() const
		{
			uint32_t a = 1;
			uint32_t b = 0;

			for (uint8_t byte : m_decoded)
			{
				a = (a + byte) % 65521;
				b = (b + a) % 65521;
			}

			return (b << 16) | a;
		}

		BAN::ErrorOr<bool> decode_block()
		{
			bool bfinal = TRY(m_buffer.get_bits(1));
			uint8_t btype = TRY(m_buffer.get_bits(2));

			switch (btype)
			{
				case 0: TRY(decode_type0()); break;
				case 1: TRY(decode_type1()); break;
				case 2: TRY(decode_type2()); break;
				default:
					dwarnln_if(DEBUG_PNG, "Deflate block has invalid method {}", btype);
					return BAN::Error::from_errno(EINVAL);
			}

			return bfinal;
		}

		BAN::ErrorOr<void> decode_type0()
		{
			m_buffer.skip_to_byte_boundary();

			uint16_t len = TRY(m_buffer.get_bits(16));
			uint16_t nlen = TRY(m_buffer.get_bits(16));
			if (len != 0xFFFF - nlen)
			{
				dwarnln_if(DEBUG_PNG, "Deflate block uncompressed data length is invalid");
				return BAN::Error::from_errno(EINVAL);
			}

			TRY(m_decoded.reserve(m_decoded.size() + len));
			for (uint16_t i = 0; i < len; i++)
				MUST(m_decoded.push_back(TRY(m_buffer.get_bits(8))));

			return {};
		}

		BAN::ErrorOr<void> decode_type1()
		{
			TRY(inflate_block(TRY(HuffmanTree::fixed_tree()), HuffmanTree()));
			return {};
		}

		BAN::ErrorOr<void> decode_type2()
		{
			static constexpr uint8_t code_length_order[] {
				16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
			};

			const uint16_t hlit  = TRY(m_buffer.get_bits(5)) + 257;
			const uint8_t  hdist = TRY(m_buffer.get_bits(5)) + 1;
			const uint8_t  hclen = TRY(m_buffer.get_bits(4)) + 4;

			HuffmanTree code_length_tree;
			{
				BAN::Vector<uint8_t> code_lengths;
				TRY(code_lengths.resize(19, 0));
				for (uint8_t i = 0; i < hclen; i++)
					code_lengths[code_length_order[i]] = TRY(m_buffer.get_bits(3));
				code_length_tree = TRY(HuffmanTree::create(code_lengths));
			}

			uint16_t last_symbol = 0;
			BAN::Vector<uint8_t> bit_lengths;
			TRY(bit_lengths.reserve(288 + 32));
			while (bit_lengths.size() < hlit + hdist)
			{
				uint16_t symbol = TRY(read_symbol(code_length_tree));
				uint8_t count = 0;

				if (symbol <= 15)
				{
					count = 1;
				}
				else if (symbol == 16)
				{
					symbol = last_symbol;
					count = TRY(m_buffer.get_bits(2)) + 3;
				}
				else if (symbol == 17)
				{
					symbol = 0;
					count = TRY(m_buffer.get_bits(3)) + 3;
				}
				else if (symbol == 18)
				{
					symbol = 0;
					count = TRY(m_buffer.get_bits(7)) + 11;
				}

				for (uint8_t i = 0; i < count; i++)
					TRY(bit_lengths.push_back(symbol));
				last_symbol = symbol;
			}

			TRY(bit_lengths.resize(hlit + 32, 0));

			BAN::Vector<uint8_t> distance_lengths;
			TRY(distance_lengths.resize(32));
			for (uint8_t i = 0; i < 32; i++)
				distance_lengths[i] = bit_lengths[hlit + i];

			TRY(bit_lengths.resize(hlit));
			TRY(bit_lengths.resize(288, 0));

			TRY(inflate_block(TRY(HuffmanTree::create(bit_lengths)), TRY(HuffmanTree::create(distance_lengths))));
			return {};
		}

		BAN::ErrorOr<void> inflate_block(const HuffmanTree& length_tree, const HuffmanTree& distance_tree)
		{
			static constexpr uint16_t length_base[] {
				3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
			};
			static constexpr uint8_t extra_length_bits[] {
				0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
			};

			static constexpr uint16_t distance_base[] {
				1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
			};
			static constexpr uint8_t extra_distance_bits[] {
				0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
			};

			uint16_t symbol;
			while ((symbol = TRY(read_symbol(length_tree))) != 256)
			{
				if (symbol < 256)
				{
					TRY(m_decoded.push_back(symbol));
					continue;
				}

				ASSERT(symbol <= 285);
				symbol -= 257;

				const uint16_t length = length_base[symbol] + TRY(m_buffer.get_bits(extra_length_bits[symbol]));

				uint16_t distance_code;
				if (distance_tree.empty())
					distance_code = reverse_bits(TRY(m_buffer.get_bits(5)), 5);
				else
					distance_code = TRY(read_symbol(distance_tree));
				ASSERT(distance_code <= 30);

				const size_t distance = distance_base[distance_code] + TRY(m_buffer.get_bits(extra_distance_bits[distance_code]));

				size_t offset = m_decoded.size() - distance;
				for (size_t i = 0; i < length; i++)
					TRY(m_decoded.push_back(m_decoded[offset + i]));
			}

			return {};
		}

		BAN::ErrorOr<uint16_t> read_symbol(const HuffmanTree& tree)
		{
			uint16_t compare = TRY(m_buffer.peek_bits(tree.max_bits()));
			for (uint8_t bits = tree.instant_max_bit(); bits <= tree.max_bits(); bits++)
			{
				uint16_t mask = (1 << bits) - 1;
				auto leaf = tree.get_leaf(compare & mask);

				if (leaf.len <= bits)
				{
					m_buffer.remove_bits(leaf.len);
					return leaf.code;
				}
			}
			return BAN::Error::from_errno(EINVAL);
		}

	private:
		BAN::Vector<uint8_t> m_decoded;
		BitBuffer m_buffer;
	};

	BAN::ErrorOr<PNGChunk> read_and_take_chunk(BAN::ConstByteSpan& image_data)
	{
		if (image_data.size() < 12)
		{
			dwarnln_if(DEBUG_PNG, "PNG stream does not contain any more chunks");
			return BAN::Error::from_errno(EINVAL);
		}

		uint32_t length = image_data.as<const BAN::NetworkEndian<uint32_t>>();
		image_data = image_data.slice(4);

		if (image_data.size() < length + 8)
		{
			dwarnln_if(DEBUG_PNG, "PNG stream does not contain any more chunks");
			return BAN::Error::from_errno(EINVAL);
		}

		PNGChunk result;

		result.name = BAN::StringView(image_data.as_span<const char>().data(), 4);
		image_data = image_data.slice(4);

		result.data = image_data.slice(0, length);
		image_data = image_data.slice(length);

		// FIXME: validate CRC
		image_data = image_data.slice(4);

		return result;
	}

	static bool validate_ihdr_colour_type_and_bit_depth(const IHDR& ihdr)
	{
		if (!BAN::Math::is_power_of_two(ihdr.bit_depth))
			return false;
		switch (ihdr.colour_type)
		{
			case ColourType::Greyscale:
				if (ihdr.bit_depth < 1 || ihdr.bit_depth > 16)
					return false;
				return true;
			case ColourType::Truecolour:
				if (ihdr.bit_depth < 8 || ihdr.bit_depth > 16)
					return false;
				return true;
			case ColourType::IndexedColour:
				if (ihdr.bit_depth < 1 || ihdr.bit_depth > 8)
					return false;
				return true;
			case ColourType::GreyscaleAlpha:
				if (ihdr.bit_depth < 8 || ihdr.bit_depth > 16)
					return false;
				return true;
			case ColourType::TruecolourAlpha:
				if (ihdr.bit_depth < 8 || ihdr.bit_depth > 16)
					return false;
				return true;
			default:
				return false;
		}
	}

	bool probe_png(BAN::ConstByteSpan image_data)
	{
		if (image_data.size() < 8)
			return false;
		uint64_t u64_signature = image_data.as<const uint64_t>();
		return u64_signature == 0x0A1A0A0D474E5089;
	}

	BAN::ErrorOr<BAN::UniqPtr<Image>> load_png(BAN::ConstByteSpan image_data)
	{
		if (!probe_png(image_data))
		{
			dwarnln_if(DEBUG_PNG, "Invalid PNG data");
			return BAN::Error::from_errno(EINVAL);
		}
		image_data = image_data.slice(8);

		auto ihdr_chunk = TRY(read_and_take_chunk(image_data));
		if (ihdr_chunk.name != "IHDR")
		{
			dwarnln_if(DEBUG_PNG, "PNG stream does not start with IHDR chunk");
			return BAN::Error::from_errno(EINVAL);
		}
		if (ihdr_chunk.data.size() != sizeof(IHDR))
		{
			dwarnln_if(DEBUG_PNG, "PNG stream has invalid IHDR chunk size: {}, expected {}", ihdr_chunk.data.size(), sizeof(IHDR));
			return BAN::Error::from_errno(EINVAL);
		}

		const auto& ihdr = ihdr_chunk.data.as<const IHDR>();
		if (ihdr.width == 0 || ihdr.height == 0 || ihdr.width > 0x7FFFFFFF || ihdr.height > 0x7FFFFFFF)
		{
			dwarnln_if(DEBUG_PNG, "PNG IHDR has invalid size {}x{}", (uint32_t)ihdr.width, (uint32_t)ihdr.height);
			return BAN::Error::from_errno(EINVAL);
		}
		if (!validate_ihdr_colour_type_and_bit_depth(ihdr))
		{
			dwarnln_if(DEBUG_PNG, "PNG IHDR has invalid bit depth {} for colour type {}", ihdr.bit_depth, static_cast<uint8_t>(ihdr.colour_type));
			return BAN::Error::from_errno(EINVAL);
		}
		if (ihdr.compression_method != CompressionMethod::Deflate)
		{
			dwarnln_if(DEBUG_PNG, "PNG IHDR has invalid compression method {}", static_cast<uint8_t>(ihdr.compression_method));
			return BAN::Error::from_errno(EINVAL);
		}
		if (ihdr.filter_method != FilterMethod::Adaptive)
		{
			dwarnln_if(DEBUG_PNG, "PNG IHDR has invalid filter method {}", static_cast<uint8_t>(ihdr.filter_method));
			return BAN::Error::from_errno(EINVAL);
		}
		if (ihdr.interlace_method != InterlaceMethod::NoInterlace && ihdr.interlace_method != InterlaceMethod::Adam7)
		{
			dwarnln_if(DEBUG_PNG, "PNG IHDR has invalid interlace method {}", static_cast<uint8_t>(ihdr.interlace_method));
			return BAN::Error::from_errno(EINVAL);
		}

		if (ihdr.interlace_method == InterlaceMethod::Adam7)
		{
			dwarnln_if(DEBUG_PNG, "PNG with interlacing is not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}

		const uint64_t image_width = ihdr.width;
		const uint64_t image_height = ihdr.height;

		dprintln_if(DEBUG_PNG, "Decoding {}x{} PNG image", image_width, image_height);
		dprintln_if(DEBUG_PNG, "  bit depth:          {}", ihdr.bit_depth);
		dprintln_if(DEBUG_PNG, "  colour type:        {}", static_cast<uint8_t>(ihdr.colour_type));
		dprintln_if(DEBUG_PNG, "  compression method: {}", static_cast<uint8_t>(ihdr.compression_method));
		dprintln_if(DEBUG_PNG, "  filter method:      {}", static_cast<uint8_t>(ihdr.filter_method));
		dprintln_if(DEBUG_PNG, "  interlace method:   {}", static_cast<uint8_t>(ihdr.interlace_method));

		BAN::Vector<Image::Color> palette;
		BAN::Vector<BAN::ConstByteSpan> zlib_stream;

		while (true)
		{
			PNGChunk chunk;
			if (auto ret = read_and_take_chunk(image_data); !ret.is_error())
				chunk = ret.release_value();
			else
			{
				dwarnln_if(DEBUG_PNG, "PNG stream does not end with IEND chunk");
				return BAN::Error::from_errno(EINVAL);
			}

			if (chunk.name == "IHDR"sv)
			{
				dwarnln_if(DEBUG_PNG, "PNG stream has IDHR chunk defined multiple times");
				return BAN::Error::from_errno(EINVAL);
			}
			else if (chunk.name == "PLTE"sv)
			{
				if (chunk.data.size() == 0 || chunk.data.size() % 3)
				{
					dwarnln_if(DEBUG_PNG, "PNG PLTE has invalid data size {}", chunk.data.size());
					return BAN::Error::from_errno(EINVAL);
				}
				if (!palette.empty())
				{
					dwarnln_if(DEBUG_PNG, "PNG PLTE defined multiple times");
					return BAN::Error::from_errno(EINVAL);
				}
				if (ihdr.colour_type != ColourType::IndexedColour && ihdr.colour_type != ColourType::Truecolour && ihdr.colour_type != ColourType::TruecolourAlpha)
				{
					dwarnln_if(DEBUG_PNG, "PNG PLTE defined for colour type {} which does not use palette", static_cast<uint8_t>(ihdr.colour_type));
					return BAN::Error::from_errno(EINVAL);
				}
				TRY(palette.resize(chunk.data.size() / 3));
				for (size_t i = 0; i < palette.size(); i += 3)
				{
					palette[i].r = chunk.data[i + 0];
					palette[i].g = chunk.data[i + 1];
					palette[i].b = chunk.data[i + 2];
					palette[i].a = 0xFF;
				}
			}
			else if (chunk.name == "IDAT"sv)
			{
				TRY(zlib_stream.push_back(chunk.data));
			}
			else if (chunk.name == "IEND"sv)
			{
				break;
			}
			else if (chunk.name == "tEXt"sv)
			{
				auto data_sv = BAN::StringView(chunk.data.as_span<const char>().data(), chunk.data.size());
				if (auto idx = data_sv.find('\0'); !idx.has_value())
					dwarnln_if(DEBUG_PNG, "PNG tEXt chunk does not contain null-byte");
				else
				{
					auto keyword = data_sv.substring(0, idx.value());
					auto text = data_sv.substring(idx.value() + 1);
					dprintln_if(DEBUG_PNG, "'{}': '{}'", keyword, text);
				}
			}
			else
			{
				bool ancillary = islower(chunk.name[0]);
				if (!ancillary)
				{
					dwarnln_if(DEBUG_PNG, "Unsupported critical chunk '{}'", chunk.name);
					return BAN::Error::from_errno(ENOTSUP);
				}
				dwarnln_if(DEBUG_PNG, "Skipping unsupported ancillary chunk '{}'", chunk.name);
			}
		}

		{
			if (zlib_stream.empty() || zlib_stream.front().size() < 2)
			{
				dwarnln_if(DEBUG_PNG, "PNG does not have zlib stream");
				return BAN::Error::from_errno(EINVAL);
			}
			if (zlib_stream[0].as<const BAN::BigEndian<uint16_t>>() % 31)
			{
				dwarnln_if(DEBUG_PNG, "PNG zlib stream checksum failed");
				return BAN::Error::from_errno(EINVAL);
			}

			auto zlib_header = zlib_stream[0].as<const ZLibStream>();
			if (zlib_header.fdict)
			{
				dwarnln_if(DEBUG_PNG, "PNG IDAT zlib stream has fdict set");
				return BAN::Error::from_errno(EINVAL);
			}
			if (zlib_header.cm != 8)
			{
				dwarnln_if(DEBUG_PNG, "PNG IDAT has invalid zlib compression method {}", (uint8_t)zlib_header.cm);
				return BAN::Error::from_errno(EINVAL);
			}
			zlib_stream[0] = zlib_stream[0].slice(2);
		}

		uint64_t total_size = 0;
		for (auto stream : zlib_stream)
			total_size += stream.size();
		dprintln_if(DEBUG_PNG, "PNG has {} byte zlib stream", total_size);

		DeflateDecoder decoder(BAN::move(zlib_stream));
		auto inflated_data = TRY(decoder.decode_stream());

		dprintln_if(DEBUG_PNG, "  uncompressed size {}", inflated_data.size());
		dprintln_if(DEBUG_PNG, "  compression ratio {}", (double)inflated_data.size() / total_size);

		uint8_t bits_per_channel = ihdr.bit_depth;
		uint8_t channels = 0;
		switch (ihdr.colour_type)
		{
			case ColourType::Greyscale:       channels = 1; break;
			case ColourType::Truecolour:      channels = 3; break;
			case ColourType::IndexedColour:   channels = 1; break;
			case ColourType::GreyscaleAlpha:  channels = 2; break;
			case ColourType::TruecolourAlpha: channels = 4; break;
			default:
				ASSERT_NOT_REACHED();
		}

		const auto extract_channel =
			[&](auto& bit_buffer) -> uint8_t
			{
				uint16_t tmp = MUST(bit_buffer.get_bits(bits_per_channel));
				switch (bits_per_channel)
				{
					case 1:  return tmp * 0xFF;
					case 2:  return tmp * 0xFF / 3;
					case 4:  return tmp * 0xFF / 15;
					case 8:  return tmp;
					case 16: return tmp & 0xFF; // NOTE: stored in big endian
				}
				ASSERT_NOT_REACHED();
			};

		const auto extract_color =
			[&](auto& bit_buffer) -> Image::Color
			{
				uint8_t tmp;
				switch (ihdr.colour_type)
				{
					case ColourType::Greyscale:
						tmp = extract_channel(bit_buffer);
						return Image::Color {
							.r = tmp,
							.g = tmp,
							.b = tmp,
							.a = 0xFF
						};
					case ColourType::Truecolour:
						return Image::Color {
							.r = extract_channel(bit_buffer),
							.g = extract_channel(bit_buffer),
							.b = extract_channel(bit_buffer),
							.a = 0xFF
						};
					case ColourType::IndexedColour:
						return palette[MUST(bit_buffer.get_bits(bits_per_channel))];
					case ColourType::GreyscaleAlpha:
						tmp = extract_channel(bit_buffer);
						return Image::Color {
							.r = tmp,
							.g = tmp,
							.b = tmp,
							.a = extract_channel(bit_buffer)
						};
					case ColourType::TruecolourAlpha:
						return Image::Color {
							.r = extract_channel(bit_buffer),
							.g = extract_channel(bit_buffer),
							.b = extract_channel(bit_buffer),
							.a = extract_channel(bit_buffer)
						};
				}
				ASSERT_NOT_REACHED();
			};

		constexpr auto paeth_predictor =
			[](int16_t a, int16_t b, int16_t c) -> uint8_t
			{
				int16_t p = a + b - c;
				int16_t pa = BAN::Math::abs(p - a);
				int16_t pb = BAN::Math::abs(p - b);
				int16_t pc = BAN::Math::abs(p - c);
				if (pa <= pb && pa <= pc)
					return a;
				if (pb <= pc)
					return b;
				return c;
			};

		const uint64_t bytes_per_scanline = BAN::Math::div_round_up<uint64_t>(image_width * channels * bits_per_channel, 8);
		const uint64_t pitch = bytes_per_scanline + 1;

		if (inflated_data.size() < pitch * image_height)
		{
			dwarnln_if(DEBUG_PNG, "PNG does not contain enough image data");
			return BAN::Error::from_errno(ENODATA);
		}

		BAN::Vector<uint8_t> zero_scanline;
		TRY(zero_scanline.resize(bytes_per_scanline, 0));

		BAN::Vector<Image::Color> color_bitmap;
		TRY(color_bitmap.resize(image_width * image_height));

		BAN::Vector<BAN::ConstByteSpan> inflated_data_wrapper;
		TRY(inflated_data_wrapper.push_back({}));

		const uint8_t filter_offset = (bits_per_channel < 8) ? 1 : channels * (bits_per_channel / 8);

		for (uint64_t y = 0; y < image_height; y++)
		{
			auto scanline       =           inflated_data.slice((y - 0) * pitch + 1, bytes_per_scanline);
			auto scanline_above = (y > 0) ? inflated_data.slice((y - 1) * pitch + 1, bytes_per_scanline) : BAN::ConstByteSpan(zero_scanline.span());

			auto filter_type = static_cast<FilterType>(inflated_data[y * pitch]);
			switch (filter_type)
			{
				case FilterType::None:
					break;
				case FilterType::Sub:
					for (uint64_t x = filter_offset; x < bytes_per_scanline; x++)
						scanline[x] += scanline[x - filter_offset];
					break;
				case FilterType::Up:
					for (uint64_t x = 0; x < bytes_per_scanline; x++)
						scanline[x] += scanline_above[x];
					break;
				case FilterType::Average:
					for (uint8_t i = 0; i < filter_offset; i++)
						scanline[i] += scanline_above[i] / 2;
					for (uint64_t x = filter_offset; x < bytes_per_scanline; x++)
						scanline[x] += ((uint16_t)scanline[x - filter_offset] + (uint16_t)scanline_above[x]) / 2;
					break;
				case FilterType::Paeth:
					for (uint8_t i = 0; i < filter_offset; i++)
						scanline[i] += paeth_predictor(0, scanline_above[i], 0);
					for (uint64_t x = filter_offset; x < bytes_per_scanline; x++)
						scanline[x] += paeth_predictor(scanline[x - filter_offset], scanline_above[x], scanline_above[x - filter_offset]);
					break;
				default:
					dwarnln_if(DEBUG_PNG, "invalid filter type {}", static_cast<uint8_t>(filter_type));
					return BAN::Error::from_errno(EINVAL);
			}

			inflated_data_wrapper[0] = scanline;
			BitBuffer bit_buffer(inflated_data_wrapper);

			for (uint64_t x = 0; x < image_width; x++)
				color_bitmap[y * image_width + x] = extract_color(bit_buffer);
		}

		return TRY(BAN::UniqPtr<Image>::create(image_width, image_height, BAN::move(color_bitmap)));
	}

}
