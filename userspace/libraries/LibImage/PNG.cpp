#include <BAN/Debug.h>
#include <BAN/Endianness.h>

#include <LibImage/PNG.h>

#include <LibDEFLATE/Decompressor.h>

#include <ctype.h>

#define DEBUG_PNG 0

// https://www.w3.org/TR/png-3/

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

	struct PNGChunk
	{
		BAN::StringView name;
		BAN::ConstByteSpan data;
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

	static BAN::ErrorOr<uint64_t> parse_pixel_data(BAN::Vector<Image::Color>& color_bitmap, uint64_t image_width, uint64_t image_height, const IHDR& ihdr, const BAN::Vector<Image::Color>& palette, BAN::ByteSpan encoded_data)
	{
		ASSERT(color_bitmap.size() >= image_height * image_width);

		const uint8_t bits_per_channel = ihdr.bit_depth;
		const uint8_t channels =
			[&]() -> uint8_t
			{
				switch (ihdr.colour_type)
				{
					case ColourType::Greyscale:       return 1;
					case ColourType::Truecolour:      return 3;
					case ColourType::IndexedColour:   return 1;
					case ColourType::GreyscaleAlpha:  return 2;
					case ColourType::TruecolourAlpha: return 4;
					default:
						ASSERT_NOT_REACHED();
				}
			}();

		const auto extract_channel =
			[&](auto& bit_buffer) -> uint8_t
			{
				uint16_t tmp = MUST(bit_buffer.take_bits(bits_per_channel));
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
				Image::Color color;
				switch (ihdr.colour_type)
				{
					case ColourType::Greyscale:
						color.r = extract_channel(bit_buffer);
						color.g = color.r;
						color.b = color.r;
						color.a = 0xFF;
						break;
					case ColourType::Truecolour:
						color.r = extract_channel(bit_buffer);
						color.g = extract_channel(bit_buffer);
						color.b = extract_channel(bit_buffer);
						color.a = 0xFF;
						break;
					case ColourType::IndexedColour:
						color = palette[MUST(bit_buffer.take_bits(bits_per_channel))];
						break;
					case ColourType::GreyscaleAlpha:
						color.r = extract_channel(bit_buffer);
						color.g = color.r;
						color.b = color.r;
						color.a = extract_channel(bit_buffer);
						break;
					case ColourType::TruecolourAlpha:
						color.r = extract_channel(bit_buffer);
						color.g = extract_channel(bit_buffer);
						color.b = extract_channel(bit_buffer);
						color.a = extract_channel(bit_buffer);
						break;
				}
				return color;
			};

		constexpr auto paeth_predictor =
			[](int16_t a, int16_t b, int16_t c) -> uint8_t
			{
				const int16_t p = a + b - c;
				const int16_t pa = BAN::Math::abs(p - a);
				const int16_t pb = BAN::Math::abs(p - b);
				const int16_t pc = BAN::Math::abs(p - c);
				if (pa <= pb && pa <= pc)
					return a;
				if (pb <= pc)
					return b;
				return c;
			};

		const uint64_t bytes_per_scanline = BAN::Math::div_round_up<uint64_t>(image_width * channels * bits_per_channel, 8);
		const uint64_t pitch = bytes_per_scanline + 1;

		if (encoded_data.size() < pitch * image_height)
		{
			dwarnln_if(DEBUG_PNG, "PNG does not contain enough image data");
			return BAN::Error::from_errno(ENODATA);
		}

		BAN::Vector<uint8_t> zero_scanline;
		TRY(zero_scanline.resize(bytes_per_scanline, 0));

		const uint8_t filter_offset = (bits_per_channel < 8) ? 1 : channels * (bits_per_channel / 8);

		for (uint64_t y = 0; y < image_height; y++)
		{
			auto scanline       =           encoded_data.slice((y - 0) * pitch + 1, bytes_per_scanline);
			auto scanline_above = (y > 0) ? encoded_data.slice((y - 1) * pitch + 1, bytes_per_scanline) : BAN::ConstByteSpan(zero_scanline.span());

			auto filter_type = static_cast<FilterType>(encoded_data[y * pitch]);
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

			LibDEFLATE::BitInputStream bit_stream(scanline);
			for (uint64_t x = 0; x < image_width; x++)
				color_bitmap[y * image_width + x] = extract_color(bit_stream);
		}

		return pitch * image_height;
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

			if (chunk.name == "IHDR"_sv)
			{
				dwarnln_if(DEBUG_PNG, "PNG stream has IDHR chunk defined multiple times");
				return BAN::Error::from_errno(EINVAL);
			}
			else if (chunk.name == "PLTE"_sv)
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
				for (size_t i = 0; i < palette.size(); i++)
				{
					palette[i].r = chunk.data[3 * i + 0];
					palette[i].g = chunk.data[3 * i + 1];
					palette[i].b = chunk.data[3 * i + 2];
					palette[i].a = 0xFF;
				}
			}
			else if (chunk.name == "IDAT"_sv)
			{
				TRY(zlib_stream.push_back(chunk.data));
			}
			else if (chunk.name == "IEND"_sv)
			{
				break;
			}
			else if (chunk.name == "tEXt"_sv)
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

		BAN::Vector<uint8_t> zlib_stream_buf;
		BAN::ConstByteSpan zlib_stream_span;

		if (zlib_stream.empty())
		{
			dwarnln_if(DEBUG_PNG, "PNG does not have zlib stream");
			return BAN::Error::from_errno(EINVAL);
		}

		if (zlib_stream.size() == 1)
			zlib_stream_span = zlib_stream.front();
		else
		{
			for (auto stream : zlib_stream)
			{
				const size_t old_size = zlib_stream_buf.size();
				TRY(zlib_stream_buf.resize(old_size + stream.size()));
				for (size_t i = 0; i < stream.size(); i++)
					zlib_stream_buf[old_size + i] = stream[i];
			}
			zlib_stream_span = zlib_stream_buf.span();
		}

		uint64_t total_size = 0;
		for (auto stream : zlib_stream)
			total_size += stream.size();
		dprintln_if(DEBUG_PNG, "PNG has {} byte zlib stream", total_size);

		LibDEFLATE::Decompressor decompressor(zlib_stream_span, LibDEFLATE::StreamType::Zlib);
		auto inflated_buffer = TRY(decompressor.decompress());
		auto inflated_data = inflated_buffer.span();

		dprintln_if(DEBUG_PNG, "  uncompressed size {}", inflated_data.size());
		dprintln_if(DEBUG_PNG, "  compression ratio {}", (double)inflated_data.size() / total_size);

		BAN::Vector<Image::Color> pixel_data;
		TRY(pixel_data.resize(image_width * image_height));

		switch (ihdr.interlace_method)
		{
			case InterlaceMethod::NoInterlace:
				TRY(parse_pixel_data(pixel_data, image_width, image_height, ihdr, palette, inflated_data));
				break;
			case InterlaceMethod::Adam7:
			{
				constexpr uint8_t x_start[]     { 0, 4, 0, 2, 0, 1, 0 };
				constexpr uint8_t x_increment[] { 8, 8, 4, 4, 2, 2, 1 };

				constexpr uint8_t y_start[]     { 0, 0, 4, 0, 2, 0, 1 };
				constexpr uint8_t y_increment[] { 8, 8, 8, 4, 4, 2, 2 };

				BAN::Vector<Image::Color> pass_pixel_data;
				TRY(pass_pixel_data.resize(((image_height + 1) / 2) * image_width));

				for (int pass = 0; pass < 7; pass++)
				{
					const uint64_t pass_width  = BAN::Math::div_round_up<uint64_t>(image_width  - x_start[pass], x_increment[pass]);
					const uint64_t pass_height = BAN::Math::div_round_up<uint64_t>(image_height - y_start[pass], y_increment[pass]);
					const uint64_t nparsed = TRY(parse_pixel_data(pass_pixel_data, pass_width, pass_height, ihdr, palette, inflated_data));

					for (uint64_t y = 0; y < pass_height; y++)
					{
						for (uint64_t x = 0; x < pass_width; x++)
						{
							const uint64_t abs_x = x * x_increment[pass] + x_start[pass];
							const uint64_t abs_y = y * y_increment[pass] + y_start[pass];
							pixel_data[abs_y * image_width + abs_x] = pass_pixel_data[y * pass_width + x];
						}
					}

					dprintln_if(DEBUG_PNG, "Adam7 pass {} done ({}x{})", pass + 1, pass_width, pass_height);
					inflated_data = inflated_data.slice(nparsed);
				}

				break;
			}
			default:
				ASSERT_NOT_REACHED();
		}

		return TRY(BAN::UniqPtr<Image>::create(image_width, image_height, BAN::move(pixel_data)));
	}

}
