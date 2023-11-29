#pragma once

#include <BAN/Vector.h>
#include <BAN/UniqPtr.h>

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
	static BAN::UniqPtr<Image> load_from_file(BAN::StringView path);

	uint64_t width() const { return m_width; }
	uint64_t height() const { return m_height; }

	bool render_to_framebuffer();

private:
	Image(uint64_t width, uint64_t height, BAN::Vector<Color>&& bitmap)
		: m_width(width)
		, m_height(height)
		, m_bitmap(BAN::move(bitmap))
	{ }

private:
	const uint64_t m_width;
	const uint64_t m_height;
	const BAN::Vector<Color> m_bitmap;

	friend class BAN::UniqPtr<Image>;
};
