#pragma once

#include <BAN/StringView.h>
#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>

namespace LibImage
{

	class Image
	{
	public:
		struct Color
		{
			uint8_t r;
			uint8_t g;
			uint8_t b;
			uint8_t a;
		};

	public:
		static BAN::ErrorOr<BAN::UniqPtr<Image>> load_from_file(BAN::StringView path);

		Color get_color(uint64_t x, uint64_t y) const { return m_bitmap[y * width() + x]; }
		const BAN::Vector<Color> bitmap() const { return m_bitmap; }

		uint64_t width() const { return m_width; }
		uint64_t height() const { return m_height; }

	private:
		Image(uint64_t width, uint64_t height, BAN::Vector<Color>&& bitmap)
			: m_width(width)
			, m_height(height)
			, m_bitmap(BAN::move(bitmap))
		{
			ASSERT(m_bitmap.size() >= width * height);
		}

	private:
		const uint64_t m_width;
		const uint64_t m_height;
		const BAN::Vector<Color> m_bitmap;

		friend class BAN::UniqPtr<Image>;
	};

}
