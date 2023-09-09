#include <BAN/Endianness.h>
#include <BAN/ScopeGuard.h>
#include <BAN/UTF8.h>
#include <kernel/Font.h>
#include <kernel/Process.h>

#include <fcntl.h>

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

extern uint8_t _binary_font_prefs_psf_start[];
extern uint8_t _binary_font_prefs_psf_end[];

namespace Kernel
{

	BAN::ErrorOr<Font> Font::prefs()
	{
		size_t font_data_size = _binary_font_prefs_psf_end - _binary_font_prefs_psf_start;
		BAN::Span<const uint8_t> font_data(_binary_font_prefs_psf_start, font_data_size);
		return parse_psf1(font_data);
	}

	BAN::ErrorOr<Font> Font::load(BAN::StringView path)
	{
		int fd = TRY(Process::current().sys_open(path, O_RDONLY));
		BAN::ScopeGuard _([fd] { MUST(Process::current().sys_close(fd)); });

		struct stat st;
		TRY(Process::current().sys_fstat(fd, &st));

		BAN::Vector<uint8_t> file_data;
		TRY(file_data.resize(st.st_size));
		TRY(Process::current().sys_read(fd, file_data.data(), st.st_size));

		if (file_data.size() < 4)
			return BAN::Error::from_error_code(ErrorCode::Font_FileTooSmall);

		if (file_data[0] == PSF1_MAGIC0 && file_data[1] == PSF1_MAGIC1)
			return TRY(parse_psf1(file_data.span()));

		if (file_data[0] == PSF2_MAGIC0 && file_data[1] == PSF2_MAGIC1 && file_data[2] == PSF2_MAGIC2 && file_data[3] == PSF2_MAGIC3)
			return TRY(parse_psf2(file_data.span()));

		return BAN::Error::from_error_code(ErrorCode::Font_Unsupported);
	}

	BAN::ErrorOr<Font> Font::parse_psf1(BAN::Span<const uint8_t> font_data)
	{
		if (font_data.size() < 4)
			return BAN::Error::from_error_code(ErrorCode::Font_FileTooSmall);

		struct PSF1Header
		{
			uint8_t magic[2];
			uint8_t mode;
			uint8_t char_size;
		};
		const PSF1Header& header = *(const PSF1Header*)font_data.data();
		
		uint32_t glyph_count = header.mode & PSF1_MODE512 ? 512 : 256;
		uint32_t glyph_size = header.char_size;
		uint32_t glyph_data_size = glyph_size * glyph_count;

		if (font_data.size() < sizeof(PSF1Header) + glyph_data_size)
			return BAN::Error::from_error_code(ErrorCode::Font_FileTooSmall);

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
			dwarnln("Font contsins multiple definitions for same codepoint(s)");
		if (codepoint_sequence)
			dwarnln("Font contains codepoint sequences (not supported)");

		Font result;
		result.m_glyph_offsets = BAN::move(glyph_offsets);
		result.m_glyph_data = BAN::move(glyph_data);
		result.m_width = 8;
		result.m_height = header.char_size;
		result.m_pitch = 1;
		return result;
	}

	BAN::ErrorOr<Font> Font::parse_psf2(BAN::Span<const uint8_t> font_data)
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
			return BAN::Error::from_error_code(ErrorCode::Font_FileTooSmall);

		const PSF2Header& header = *(const PSF2Header*)font_data.data();

		uint32_t glyph_data_size = header.glyph_count * header.glyph_size;

		if (font_data.size() < glyph_data_size + header.header_size)
			return BAN::Error::from_error_code(ErrorCode::Font_FileTooSmall);

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

					if (len == 0)
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