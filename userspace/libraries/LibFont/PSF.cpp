#include <BAN/Debug.h>
#include <BAN/Endianness.h>
#include <BAN/UTF8.h>

#include <LibFont/PSF.h>

#define PSF1_MAGIC0				0x36
#define PSF1_MAGIC1				0x04
#define PSF1_MODE512			0x01
#define PSF1_MODEHASTAB			0x02
#define PSF1_MODEHASSEQ			0x04
#define PSF1_STARTSEQ			0xFFFE
#define PSF1_SEPARATOR			0xFFFF

#define PSF2_MAGIC0				0x72
#define PSF2_MAGIC1				0xB5
#define PSF2_MAGIC2				0x4A
#define PSF2_MAGIC3				0x86
#define PSF2_HAS_UNICODE_TABLE	0x01
#define PSF2_STARTSEQ			0xFE
#define PSF2_SEPARATOR			0xFF

namespace LibFont
{

	bool is_psf1(BAN::ConstByteSpan font_data)
	{
		if (font_data.size() < 2)
			return false;
		return font_data[0] == PSF1_MAGIC0 && font_data[1] == PSF1_MAGIC1;
	}

	BAN::ErrorOr<Font> parse_psf1(BAN::ConstByteSpan font_data)
	{
		struct PSF1Header
		{
			uint8_t magic[2];
			uint8_t mode;
			uint8_t char_size;
		};

		if (font_data.size() < sizeof(PSF1Header))
			return BAN::Error::from_errno(EINVAL);
		const auto& header = font_data.as<const PSF1Header>();

		uint32_t glyph_count = header.mode & PSF1_MODE512 ? 512 : 256;
		uint32_t glyph_size = header.char_size;
		uint32_t glyph_data_size = glyph_size * glyph_count;

		if (font_data.size() < sizeof(PSF1Header) + glyph_data_size)
			return BAN::Error::from_errno(EINVAL);

		BAN::Vector<uint8_t> glyph_data;
		TRY(glyph_data.resize(glyph_data_size));
		memcpy(glyph_data.data(), font_data.data() + sizeof(PSF1Header), glyph_data_size);

		BAN::HashMap<uint32_t, uint32_t> glyph_offsets;
		TRY(glyph_offsets.reserve(glyph_count));

		bool codepoint_redef = false;
		bool codepoint_sequence = false;

		if (header.mode & (PSF1_MODEHASTAB | PSF1_MODEHASSEQ))
		{
			uint32_t current_index = sizeof(PSF1Header) + glyph_data_size;

			uint32_t glyph_index = 0;
			while (current_index < font_data.size())
			{
				uint16_t lo = font_data[current_index];
				uint16_t hi = font_data[current_index + 1];
				uint16_t codepoint = (hi << 8) | lo;

				if (codepoint == PSF1_STARTSEQ)
				{
					codepoint_sequence = true;
					break;
				}
				else if (codepoint == PSF1_SEPARATOR)
				{
					glyph_index++;
				}
				else
				{
					if (glyph_offsets.contains(codepoint))
						codepoint_redef = true;
					else
						TRY(glyph_offsets.insert(codepoint, glyph_index * glyph_size));
				}

				current_index += 2;
			}
		}
		else
		{
			for (uint32_t i = 0; i < glyph_count; i++)
				TRY(glyph_offsets.insert(i, i * glyph_size));
		}

		if (codepoint_redef)
			dwarnln("Font contains multiple definitions for same codepoint(s)");
		if (codepoint_sequence)
			dwarnln("Font contains codepoint sequences (not supported)");

		return Font(BAN::move(glyph_offsets), BAN::move(glyph_data), 8, header.char_size, 1);
	}

	bool is_psf2(BAN::ConstByteSpan font_data)
	{
		if (font_data.size() < 4)
			return false;
		return font_data[0] == PSF2_MAGIC0 && font_data[1] == PSF2_MAGIC1 && font_data[2] == PSF2_MAGIC2 && font_data[3] == PSF2_MAGIC3;
	}

	BAN::ErrorOr<Font> parse_psf2(BAN::ConstByteSpan font_data)
	{
		struct PSF2Header
		{
			uint8_t magic[4];
			BAN::LittleEndian<uint32_t> version;
			BAN::LittleEndian<uint32_t> header_size;
			BAN::LittleEndian<uint32_t> flags;
			BAN::LittleEndian<uint32_t> glyph_count;
			BAN::LittleEndian<uint32_t> glyph_size;
			BAN::LittleEndian<uint32_t> height;
			BAN::LittleEndian<uint32_t> width;
		};

		if (font_data.size() < sizeof(PSF2Header))
			return BAN::Error::from_errno(EINVAL);
		const auto& header = font_data.as<const PSF2Header>();

		uint32_t glyph_data_size = header.glyph_count * header.glyph_size;

		if (font_data.size() < glyph_data_size + header.header_size)
			return BAN::Error::from_errno(EINVAL);

		BAN::Vector<uint8_t> glyph_data;
		TRY(glyph_data.resize(glyph_data_size));
		memcpy(glyph_data.data(), font_data.data() + header.header_size, glyph_data_size);

		BAN::HashMap<uint32_t, uint32_t> glyph_offsets;
		TRY(glyph_offsets.reserve(400));

		bool invalid_utf = false;
		bool codepoint_redef = false;
		bool codepoint_sequence = false;

		uint8_t bytes[4] {};
		uint32_t byte_index = 0;
		if (header.flags & PSF2_HAS_UNICODE_TABLE)
		{
			uint32_t glyph_index = 0;
			for (uint32_t i = glyph_data_size + header.header_size; i < font_data.size(); i++)
			{
				uint8_t byte = font_data[i];

				if (byte == PSF2_STARTSEQ)
				{
					codepoint_sequence = true;
					break;
				}
				else if (byte == PSF2_SEPARATOR)
				{
					if (byte_index)
					{
						invalid_utf = true;
						byte_index = 0;
					}
					glyph_index++;
				}
				else
				{
					ASSERT(byte_index < 4);
					bytes[byte_index++] = byte;

					uint32_t len = BAN::UTF8::byte_length(bytes[0]);

					if (len == BAN::UTF8::invalid)
					{
						invalid_utf = true;
						byte_index = 0;
					}
					else if (len == byte_index)
					{
						uint32_t codepoint = BAN::UTF8::to_codepoint(bytes);
						if (codepoint == BAN::UTF8::invalid)
							invalid_utf = true;
						else if (glyph_offsets.contains(codepoint))
							codepoint_redef = true;
						else
							TRY(glyph_offsets.insert(codepoint, glyph_index * header.glyph_size));
						byte_index = 0;
					}
				}
			}
		}
		else
		{
			for (uint32_t i = 0; i < header.glyph_count; i++)
				TRY(glyph_offsets.insert(i, i * header.glyph_size));
		}

		if (invalid_utf)
			dwarnln("Font contains invalid UTF-8 codepoint(s)");
		if (codepoint_redef)
			dwarnln("Font contains multiple definitions for same codepoint(s)");
		if (codepoint_sequence)
			dwarnln("Font contains codepoint sequences (not supported)");

		return Font(BAN::move(glyph_offsets), BAN::move(glyph_data), header.width, header.height, header.glyph_size / header.height);
	}

}
