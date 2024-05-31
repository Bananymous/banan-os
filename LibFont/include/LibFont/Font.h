#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/HashMap.h>
#include <BAN/StringView.h>

namespace LibFont
{

	class Font
	{
	public:
		static BAN::ErrorOr<Font> load(BAN::StringView path);
#if __is_kernel
		static BAN::ErrorOr<Font> prefs();
#endif

		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }
		uint32_t pitch() const { return m_pitch; }

		bool has_glyph(uint32_t codepoint) const { return glyph(codepoint) != nullptr; }
		const uint8_t* glyph(uint32_t codepoint) const
		{
			auto it = m_glyph_offsets.find(codepoint);
			if (it == m_glyph_offsets.end())
				return nullptr;
			return m_glyph_data.data() + it->value;
		}

	private:
		static BAN::ErrorOr<Font> parse_psf1(BAN::ConstByteSpan);
		static BAN::ErrorOr<Font> parse_psf2(BAN::ConstByteSpan);

	private:
		BAN::HashMap<uint32_t, uint32_t> m_glyph_offsets;
		BAN::Vector<uint8_t> m_glyph_data;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_pitch = 0;
	};

}
