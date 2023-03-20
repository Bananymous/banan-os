#include <BAN/ScopeGuard.h>
#include <BAN/UTF8.h>
#include <kernel/Font.h>
#include <kernel/Process.h>

#include <fcntl.h>

#define PSF1_MODE_512			0x01
#define PSF1_MODE_HASTAB		0x02
#define PSF1_MODE_SEQ			0x02

#define PSF2_HAS_UNICODE_TABLE	0x00000001

extern uint8_t _binary_font_prefs_psf_start[];
extern uint8_t _binary_font_prefs_psf_end[];

namespace Kernel
{

	BAN::ErrorOr<Font> Font::prefs()
	{
		size_t font_data_size = _binary_font_prefs_psf_end - _binary_font_prefs_psf_start;
		BAN::Span<uint8_t> font_data(_binary_font_prefs_psf_start, font_data_size);
		return parse_psf1(font_data);
	}

	BAN::ErrorOr<Font> Font::load(BAN::StringView path)
	{
		int fd = TRY(Process::current()->open(path, O_RDONLY));
		BAN::ScopeGuard _([fd] { MUST(Process::current()->close(fd)); });

		size_t file_size = Process::current()->inode_for_fd(fd).size();
		BAN::Vector<uint8_t> file_data;
		TRY(file_data.resize(file_size));
		TRY(Process::current()->read(fd, file_data.data(), file_size));

		if (file_data.size() < 4)
			return BAN::Error::from_c_string("Font file is too small");

		if (file_data[0] == 0x36 && file_data[1] == 0x04)
			return TRY(parse_psf1(file_data.span()));

		if (file_data[0] == 0x72 && file_data[1] == 0xB5 && file_data[2] == 0x4A && file_data[3] == 0x86)
			return TRY(parse_psf2(file_data.span()));

		return BAN::Error::from_c_string("Unsupported font format");
	}


	BAN::ErrorOr<Font> Font::parse_psf1(const BAN::Span<uint8_t> font_data)
	{
		if (font_data.size() < 4)
			return BAN::Error::from_c_string("Font file is too small");

		struct PSF1Header
		{
			uint16_t magic;
			uint8_t mode;
			uint8_t char_size;
		};
		auto* header = (const PSF1Header*)(font_data.data());
		
		uint32_t glyph_count = header->mode & PSF1_MODE_512 ? 512 : 256;
		uint32_t glyph_size = header->char_size;
		uint32_t glyph_data_size = glyph_size * glyph_count;

		if (font_data.size() < sizeof(PSF1Header) + glyph_data_size)
			return BAN::Error::from_c_string("Font file is too small");

		BAN::Vector<uint8_t> glyph_data;
		TRY(glyph_data.resize(glyph_data_size));
		memcpy(glyph_data.data(), font_data.data() + sizeof(PSF1Header), glyph_data_size);

		BAN::HashMap<uint32_t, uint32_t> glyph_offsets;
		TRY(glyph_offsets.reserve(glyph_count));

		bool codepoint_redef = false;
		bool codepoint_sequence = false;

		if (header->magic & (PSF1_MODE_HASTAB | PSF1_MODE_SEQ))
		{
			uint32_t current_index = sizeof(PSF1Header) + glyph_data_size;

			uint32_t glyph_index = 0;
			while (current_index < font_data.size())
			{
				uint16_t lo = font_data[current_index];
				uint16_t hi = font_data[current_index + 1];
				uint16_t codepoint = (hi << 8) | lo;

				if (codepoint == 0xFFFE)
				{
					codepoint_sequence = true;
					break;
				}
				else if (codepoint == 0xFFFF)
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
			dwarnln("Font contsins multiple definitions for same codepoint(s)");
		if (codepoint_sequence)
			dwarnln("Font contains codepoint sequences (not supported)");

		Font result;
		result.m_glyph_offsets = BAN::move(glyph_offsets);
		result.m_glyph_data = BAN::move(glyph_data);
		result.m_width = 8;
		result.m_height = header->char_size;
		result.m_pitch = 1;
		return result;
	}

	BAN::ErrorOr<Font> Font::parse_psf2(const BAN::Span<uint8_t> font_data)
	{
		struct PSF2Header
		{
			uint32_t magic;
			uint32_t version;
			uint32_t header_size;
			uint32_t flags;
			uint32_t glyph_count;
			uint32_t glyph_size;
			uint32_t height;
			uint32_t width;
		};

		if (font_data.size() < sizeof(PSF2Header))
			return BAN::Error::from_c_string("Font file is too small");

		PSF2Header header;
		header.magic		= BAN::Math::little_endian_to_host<uint32_t>(font_data.data() + 0);
		header.version		= BAN::Math::little_endian_to_host<uint32_t>(font_data.data() + 4);
		header.header_size	= BAN::Math::little_endian_to_host<uint32_t>(font_data.data() + 8);
		header.flags		= BAN::Math::little_endian_to_host<uint32_t>(font_data.data() + 12);
		header.glyph_count	= BAN::Math::little_endian_to_host<uint32_t>(font_data.data() + 16);
		header.glyph_size	= BAN::Math::little_endian_to_host<uint32_t>(font_data.data() + 20);
		header.height		= BAN::Math::little_endian_to_host<uint32_t>(font_data.data() + 24);
		header.width		= BAN::Math::little_endian_to_host<uint32_t>(font_data.data() + 28);

		uint32_t glyph_data_size = header.glyph_count * header.glyph_size;

		if (font_data.size() < glyph_data_size + header.header_size)
			return BAN::Error::from_c_string("Font file is too small");

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

				if (byte == 0xFE)
				{
					codepoint_sequence = true;
					break;
				}
				else if (byte == 0xFF)
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
					uint32_t len = BAN::utf8_byte_length(bytes[0]);

					if (len == 0)
					{
						invalid_utf = true;
						byte_index = 0;
					}
					else if (len == byte_index)
					{
						uint32_t codepoint = BAN::utf8_to_codepoint(bytes);
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
			dwarnln("Font contsins multiple definitions for same codepoint(s)");
		if (codepoint_sequence)
			dwarnln("Font contains codepoint sequences (not supported)");

		Font result;
		result.m_glyph_offsets = BAN::move(glyph_offsets);
		result.m_glyph_data = BAN::move(glyph_data);
		result.m_width = header.width;
		result.m_height = header.height;
		result.m_pitch = header.glyph_size / header.height;
		return result;
	}
	
	bool Font::has_glyph(uint32_t codepoint) const
	{
		return m_glyph_offsets.contains(codepoint);
	}

	const uint8_t* Font::glyph(uint32_t codepoint) const
	{
		return m_glyph_data.data() + m_glyph_offsets[codepoint];
	}

}