#include <BAN/UTF8.h>
#include <kernel/Font.h>
#include <kernel/FS/VirtualFileSystem.h>

#define PSF1_MODE_512			0x01
#define PSF1_MODE_HASTAB		0x02
#define PSF1_MODE_SEQ			0x02

#define PSF2_HAS_UNICODE_TABLE	0x00000001

extern char _binary_font_prefs_psf_start;
extern char _binary_font_prefs_psf_end;

namespace Kernel
{

	BAN::ErrorOr<Font> Font::prefs()
	{
		size_t font_data_size = &_binary_font_prefs_psf_end - &_binary_font_prefs_psf_start;
		BAN::Vector<uint8_t> font_data;
		TRY(font_data.resize(font_data_size));
		memcpy(font_data.data(), &_binary_font_prefs_psf_start, font_data_size);
		return parse_psf1(font_data);
	}

	BAN::ErrorOr<Font> Font::load(BAN::StringView path)
	{
		if (!VirtualFileSystem::is_initialized())
			return BAN::Error::from_string("Virtual Filesystem is not initialized");

		auto inode = TRY(VirtualFileSystem::get().from_absolute_path(path));

		auto file_data = TRY(inode->read_all());

		if (file_data.size() < 4)
			return BAN::Error::from_string("Font file is too small");

		if (file_data[0] == 0x36 && file_data[1] == 0x04)
			return TRY(parse_psf1(file_data));

		if (file_data[0] == 0x72 && file_data[1] == 0xB5 && file_data[2] == 0x4A && file_data[3] == 0x86)
			return TRY(parse_psf2(file_data));

		return BAN::Error::from_string("Unsupported font format");
	}


	BAN::ErrorOr<Font> Font::parse_psf1(const BAN::Vector<uint8_t>& font_data)
	{
		if (font_data.size() < 4)
			return BAN::Error::from_string("Font file is too small");

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
			return BAN::Error::from_string("Font file is too small");

		BAN::Vector<uint8_t> glyph_data;
		TRY(glyph_data.resize(glyph_data_size));
		memcpy(glyph_data.data(), font_data.data() + sizeof(PSF1Header), glyph_data_size);

		BAN::HashMap<uint16_t, uint32_t> glyph_offsets;
		TRY(glyph_offsets.reserve(glyph_count));

		bool unsupported_utf = false;
		bool codepoint_redef = false;

		if (header->magic & (PSF1_MODE_HASTAB | PSF1_MODE_SEQ))
		{
			uint32_t current_index = sizeof(PSF1Header) + glyph_data_size;

			bool in_sequence = false;
			uint32_t glyph_index = 0;
			while (current_index < font_data.size())
			{
				uint16_t lo = font_data[current_index];
				uint16_t hi = font_data[current_index + 1];
				uint16_t codepoint = (hi << 8) | lo;

				if (codepoint == 0xFFFF)
				{
					glyph_index++;
					in_sequence = false;
				}
				else if (codepoint == 0xFFFE)
				{
					in_sequence = true;
					unsupported_utf = true;
				}
				else if (!in_sequence)
				{
					if (glyph_offsets.contains(codepoint))
						codepoint_redef = true;
					else
						TRY(glyph_offsets.insert(codepoint, glyph_index * glyph_size));
				}

				current_index += 2;
			}

			if (glyph_index != glyph_count)
				return BAN::Error::from_string("Font did not contain unicode entry for all glyphs");
		}
		else
		{
			for (uint32_t i = 0; i < glyph_count; i++)
				TRY(glyph_offsets.insert(i, i * glyph_size));
		}

		if (unsupported_utf)
			dwarnln("Font contains invalid/unsupported UTF-8 codepoint(s)");
		if (codepoint_redef)
			dwarnln("Font contsins multiple definitions for same codepoint(s)");

		Font result;
		result.m_glyph_offsets = BAN::move(glyph_offsets);
		result.m_glyph_data = BAN::move(glyph_data);
		result.m_width = 8;
		result.m_height = header->char_size;
		result.m_pitch = 1;
		return result;
	}

	BAN::ErrorOr<Font> Font::parse_psf2(const BAN::Vector<uint8_t>& font_data)
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
			return BAN::Error::from_string("Font file is too small");

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
			return BAN::Error::from_string("Font file is too small");

		BAN::Vector<uint8_t> glyph_data;
		TRY(glyph_data.resize(glyph_data_size));
		memcpy(glyph_data.data(), font_data.data() + header.header_size, glyph_data_size);

		BAN::HashMap<uint16_t, uint32_t> glyph_offsets;
		TRY(glyph_offsets.reserve(400));

		bool unsupported_utf = false;
		bool codepoint_redef = false;

		uint8_t bytes[4] {};
		uint32_t byte_index = 0;
		if (header.flags & PSF2_HAS_UNICODE_TABLE)
		{
			uint32_t glyph_index = 0;
			for (uint32_t i = glyph_data_size + header.header_size; i < font_data.size(); i++)
			{
				uint8_t byte = font_data[i];

				if ((byte >> 1) == 0x7F)
				{
					if (byte_index <= 4)
					{
						uint16_t codepoint = BAN::utf8_to_codepoint(bytes, byte_index);
						if (codepoint == 0xFFFF)
							unsupported_utf = true;
						else if (glyph_offsets.contains(codepoint))
							codepoint_redef = true;
						else
							TRY(glyph_offsets.insert(codepoint, glyph_index * header.glyph_size));
					}
					byte_index = 0;
					if (byte == 0xFF)
						glyph_index++;
				}
				else
				{
					if (byte_index < 4)
						bytes[byte_index++] = byte;
					else
						unsupported_utf = true;
				}
			}
			if (glyph_index != header.glyph_count)
				return BAN::Error::from_string("Font did not contain unicode entry for all glyphs");
		}
		else
		{
			for (uint32_t i = 0; i < header.glyph_count; i++)
				TRY(glyph_offsets.insert(i, i * header.glyph_size));	
		}

		// Manually add space (empty) character if it is not present
		if (!glyph_offsets.contains(' '))
		{
			TRY(glyph_data.resize(glyph_data_size + header.glyph_size));
			memset(glyph_data.data() + glyph_data_size, 0, header.glyph_size);
			TRY(glyph_offsets.insert(' ', glyph_data_size));
		}

		if (unsupported_utf)
			dwarnln("Font contains invalid/unsupported UTF-8 codepoint(s)");
		if (codepoint_redef)
			dwarnln("Font contsins multiple definitions for same codepoint(s)");

		Font result;
		result.m_glyph_offsets = BAN::move(glyph_offsets);
		result.m_glyph_data = BAN::move(glyph_data);
		result.m_width = header.width;
		result.m_height = header.height;
		result.m_pitch = header.glyph_size / header.height;
		return result;
	}
	
	bool Font::has_glyph(uint16_t codepoint) const
	{
		return m_glyph_offsets.contains(codepoint);
	}

	const uint8_t* Font::glyph(uint16_t codepoint) const
	{
		return m_glyph_data.data() + m_glyph_offsets[codepoint];
	}

}