#pragma once

#include <BAN/HashMap.h>
#include <BAN/Span.h>
#include <BAN/StringView.h>

namespace Kernel
{

	class Font
	{
	public:
		static BAN::ErrorOr<Font> load(BAN::StringView);
		static BAN::ErrorOr<Font> prefs();

		uint32_t width() const { return m_width; }
		uint32_t height() const { return m_height; }
		uint32_t pitch() const { return m_pitch; }

		bool has_glyph(uint32_t) const;
		const uint8_t* glyph(uint32_t) const;

	private:
		static BAN::ErrorOr<Font> parse_psf1(BAN::Span<const uint8_t>);
		static BAN::ErrorOr<Font> parse_psf2(BAN::Span<const uint8_t>);

	private:
		BAN::HashMap<uint32_t, uint32_t> m_glyph_offsets;
		BAN::Vector<uint8_t> m_glyph_data;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_pitch = 0;
	};

}