#include <BAN/ScopeGuard.h>
#include <BAN/String.h>

#include <LibImage/Image.h>
#include <LibImage/Netbpm.h>
#include <LibImage/PNG.h>

#include <fcntl.h>
#include <sys/mman.h>

#include <immintrin.h>

namespace LibImage
{

	BAN::ErrorOr<BAN::UniqPtr<Image>> Image::load_from_file(BAN::StringView path)
	{
		int fd = -1;

		if (path.data()[path.size()] == '\0')
		{
			fd = open(path.data(), O_RDONLY);
		}
		else
		{
			BAN::String path_str;
			TRY(path_str.append(path));
			fd = open(path_str.data(), O_RDONLY);
		}

		if (fd == -1)
		{
			fprintf(stddbg, "open: %s\n", strerror(errno));
			return BAN::Error::from_errno(errno);
		}

		BAN::ScopeGuard guard_file_close([fd] { close(fd); });

		struct stat st;
		if (fstat(fd, &st) == -1)
		{
			fprintf(stddbg, "fstat: %s\n", strerror(errno));
			return BAN::Error::from_errno(errno);
		}

		void* addr = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (addr == MAP_FAILED)
		{
			fprintf(stddbg, "mmap: %s\n", strerror(errno));
			return BAN::Error::from_errno(errno);
		}

		BAN::ScopeGuard guard_munmap([&] { munmap(addr, st.st_size); });

		auto image_data_span = BAN::ConstByteSpan(reinterpret_cast<uint8_t*>(addr), st.st_size);

		if (probe_netbpm(image_data_span))
			return TRY(load_netbpm(image_data_span));

		if (probe_png(image_data_span))
			return TRY(load_png(image_data_span));

		fprintf(stderr, "unrecognized image format\n");
		return BAN::Error::from_errno(ENOTSUP);
	}

	struct FloatingColor
	{
		__m128 vals;

		FloatingColor() {}
		FloatingColor(float b, float g, float r, float a)
			: vals { b, g, r, a }
		{}
		FloatingColor(Image::Color c)
			: FloatingColor(c.b, c.g, c.r, c.a)
		{}
		FloatingColor operator*(float value) const
		{
			FloatingColor color;
   			color.vals = _mm_mul_ps(vals, _mm_set1_ps(value));
			return color;
		}
		FloatingColor operator+(FloatingColor other) const
		{
			FloatingColor color;
			color.vals = _mm_add_ps(this->vals, other.vals);
			return color;
		}
		Image::Color as_color() const
		{
			__m128i int32 = _mm_cvttps_epi32(this->vals);
			__m128i int16 = _mm_packs_epi32(int32, _mm_setzero_si128());
			__m128i int8 = _mm_packus_epi16(int16, _mm_setzero_si128());

			const uint32_t temp = _mm_cvtsi128_si32(int8);
			return Image::Color {
				.b = reinterpret_cast<const uint8_t*>(&temp)[0],
				.g = reinterpret_cast<const uint8_t*>(&temp)[1],
				.r = reinterpret_cast<const uint8_t*>(&temp)[2],
				.a = reinterpret_cast<const uint8_t*>(&temp)[3],
			};
		}
	};

	BAN::ErrorOr<BAN::UniqPtr<Image>> Image::resize(uint64_t new_width, uint64_t new_height, ResizeAlgorithm algorithm)
	{
		if (!validate_size(new_width, new_height))
			return BAN::Error::from_errno(EOVERFLOW);

		const float ratio_x = static_cast<float>(width()) / new_width;
		const float ratio_y = static_cast<float>(height()) / new_height;

		const auto get_clamped_color =
			[this](int64_t x, int64_t y)
			{
				x = BAN::Math::clamp<int64_t>(x, 0, width() - 1);
				y = BAN::Math::clamp<int64_t>(y, 0, height() - 1);
				return get_color(x, y);
			};

		switch (algorithm)
		{
			case ResizeAlgorithm::Nearest:
			{
				BAN::Vector<Color> nearest_bitmap;
				TRY(nearest_bitmap.resize(new_width * new_height));
				for (uint64_t y = 0; y < new_height; y++)
					for (uint64_t x = 0; x < new_width; x++)
						nearest_bitmap[y * new_width + x] = get_clamped_color(x * ratio_x, y * ratio_y);
				return TRY(BAN::UniqPtr<Image>::create(new_width, new_height, BAN::move(nearest_bitmap)));
			}
			case ResizeAlgorithm::Linear:
			{
				BAN::Vector<Color> bilinear_bitmap;
				TRY(bilinear_bitmap.resize(new_width * new_height));

				const uint64_t temp_w = width()  + 1;
				const uint64_t temp_h = height() + 1;

				BAN::Vector<FloatingColor> floating_bitmap;
				TRY(floating_bitmap.resize(temp_w * temp_h));
				for (uint64_t y = 0; y < temp_h; y++)
					for (uint64_t x = 0; x < temp_w; x++)
						floating_bitmap[y * temp_w + x] = get_clamped_color(x, y);

				for (uint64_t y = 0; y < new_height; y++)
				{
					for (uint64_t x = 0; x < new_width; x++)
					{
						const float src_x = x * ratio_x;
						const float src_y = y * ratio_y;

						const float weight_x = BAN::Math::fmod(src_x, 1.0f);
						const float weight_y = BAN::Math::fmod(src_y, 1.0f);

						const uint64_t src_x_u64 = BAN::Math::clamp<uint64_t>(src_x, 0, width()  - 1);
						const uint64_t src_y_u64 = BAN::Math::clamp<uint64_t>(src_y, 0, height() - 1);

						const auto tl = floating_bitmap[(src_y_u64 + 0) * temp_w + (src_x_u64 + 0)];
						const auto tr = floating_bitmap[(src_y_u64 + 0) * temp_w + (src_x_u64 + 1)];
						const auto bl = floating_bitmap[(src_y_u64 + 1) * temp_w + (src_x_u64 + 0)];
						const auto br = floating_bitmap[(src_y_u64 + 1) * temp_w + (src_x_u64 + 1)];

						const auto avg_t = tl * (1.0f - weight_x) + tr * weight_x;
						const auto avg_b = bl * (1.0f - weight_x) + br * weight_x;
						const auto avg = avg_t * (1.0f - weight_y) + avg_b * weight_y;

						bilinear_bitmap[y * new_width + x] = avg.as_color();
					}
				}

				return TRY(BAN::UniqPtr<Image>::create(new_width, new_height, BAN::move(bilinear_bitmap)));
			}
			case ResizeAlgorithm::Cubic:
			{
				BAN::Vector<Color> bicubic_bitmap;
				TRY(bicubic_bitmap.resize(new_width * new_height, {}));

				constexpr auto cubic_interpolate =
					[](const FloatingColor p[4], float weight) -> FloatingColor
					{
						const auto a = (p[0] * -0.5) + (p[1] *  1.5) + (p[2] * -1.5) + (p[3] *  0.5);
						const auto b =  p[0]         + (p[1] * -2.5) + (p[2] *  2.0) + (p[3] * -0.5);
						const auto c = (p[0] * -0.5)                 + (p[2] *  0.5);
						const auto d =                  p[1];
						return ((a * weight + b) * weight + c) * weight + d;
					};

				const uint64_t temp_w = width() + 3;
				const uint64_t temp_h = height() + 3;

				BAN::Vector<FloatingColor> floating_bitmap;
				TRY(floating_bitmap.resize(temp_w * temp_h, {}));
				for (uint64_t y = 0; y < temp_h; y++)
					for (uint64_t x = 0; x < temp_w; x++)
						floating_bitmap[y * temp_w + x] = get_clamped_color(
							static_cast<int64_t>(x) - 1,
							static_cast<int64_t>(y) - 1
						);

				for (uint64_t y = 0; y < new_height; y++)
				{
					for (uint64_t x = 0; x < new_width; x++)
					{
						const float src_x = x * ratio_x;
						const float src_y = y * ratio_y;

						const float weight_x = BAN::Math::fmod(src_x, 1.0f);
						const float weight_y = BAN::Math::fmod(src_y, 1.0f);

						const uint64_t src_x_u64 = BAN::Math::clamp<uint64_t>(src_x, 0, width()  - 1) + 1;
						const uint64_t src_y_u64 = BAN::Math::clamp<uint64_t>(src_y, 0, height() - 1) + 1;

						FloatingColor values[4];
						for (int64_t m = -1; m <= 2; m++)
						{
							const FloatingColor p[4] {
								floating_bitmap[(src_y_u64 + m) * temp_w + (src_x_u64 - 1)],
								floating_bitmap[(src_y_u64 + m) * temp_w + (src_x_u64 + 0)],
								floating_bitmap[(src_y_u64 + m) * temp_w + (src_x_u64 + 1)],
								floating_bitmap[(src_y_u64 + m) * temp_w + (src_x_u64 + 2)],
							};
							values[m + 1] = cubic_interpolate(p, weight_x);
						}

						bicubic_bitmap[y * new_width + x] = cubic_interpolate(values, weight_y).as_color();
					}
				}

				return TRY(BAN::UniqPtr<Image>::create(new_width, new_height, BAN::move(bicubic_bitmap)));
			}
		}

		return BAN::Error::from_errno(EINVAL);
	}

}
