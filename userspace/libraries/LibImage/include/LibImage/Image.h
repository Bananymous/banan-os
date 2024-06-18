#pragma once

#include <BAN/Limits.h>
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

			// Calculate weighted average of colors
			//   weight of 0.0 returns a and weight of 1.0 returns b
			static Color average(Color a, Color b, double weight)
			{
				const double b_mult = BAN::Math::clamp(weight, 0.0, 1.0);
				const double a_mult = 1.0 - b_mult;
				return Color {
					.r = static_cast<uint8_t>(a.r * a_mult + b.r * b_mult),
					.g = static_cast<uint8_t>(a.g * a_mult + b.g * b_mult),
					.b = static_cast<uint8_t>(a.b * a_mult + b.b * b_mult),
					.a = static_cast<uint8_t>(a.a * a_mult + b.a * b_mult),
				};
			}

			uint32_t as_rgba() const
			{
				return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
			}
		};

		enum class ResizeAlgorithm
		{
			Nearest,
			Linear,
			Cubic,
		};

	public:
		static BAN::ErrorOr<BAN::UniqPtr<Image>> load_from_file(BAN::StringView path);

		BAN::ErrorOr<BAN::UniqPtr<Image>> resize(uint64_t new_width, uint64_t new_height, ResizeAlgorithm = ResizeAlgorithm::Cubic);

		Color get_color(uint64_t x, uint64_t y) const { return m_bitmap[y * width() + x]; }
		const BAN::Vector<Color> bitmap() const { return m_bitmap; }

		uint64_t width() const { return m_width; }
		uint64_t height() const { return m_height; }

		static constexpr bool validate_size(uint64_t width, uint64_t height)
		{
			// width and height must fit in int64_t and width * height * sizeof(Color) has to not overflow
			if (width > static_cast<uint64_t>(BAN::numeric_limits<int64_t>::max()))
				return false;
			if (height > static_cast<uint64_t>(BAN::numeric_limits<int64_t>::max()))
				return false;
			if (BAN::Math::will_multiplication_overflow<uint64_t>(width, height))
				return false;
			if (BAN::Math::will_multiplication_overflow<uint64_t>(width * height, sizeof(Color)))
				return false;
			return true;
		}

	private:
		Image(uint64_t width, uint64_t height, BAN::Vector<Color>&& bitmap)
			: m_width(width)
			, m_height(height)
			, m_bitmap(BAN::move(bitmap))
		{
			ASSERT(validate_size(m_width, m_height));
			ASSERT(m_bitmap.size() >= m_width * m_height);
		}

	private:
		const uint64_t m_width;
		const uint64_t m_height;
		const BAN::Vector<Color> m_bitmap;

		friend class BAN::UniqPtr<Image>;
	};

}
