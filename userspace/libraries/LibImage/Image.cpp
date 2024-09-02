#include <BAN/ScopeGuard.h>
#include <BAN/String.h>

#include <LibImage/Image.h>
#include <LibImage/Netbpm.h>
#include <LibImage/PNG.h>

#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <sys/mman.h>

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
		double r, g, b, a;

		constexpr FloatingColor() {}
		constexpr FloatingColor(double r, double g, double b, double a)
			: r(r), g(g), b(b), a(a)
		{}
		constexpr FloatingColor(Image::Color c)
			: r(c.r), g(c.g), b(c.b), a(c.a)
		{}
		constexpr FloatingColor operator*(double value) const
		{
			return FloatingColor(r * value, g * value, b * value, a * value);
		}
		constexpr FloatingColor operator+(FloatingColor other) const
		{
			return FloatingColor(r + other.r, g + other.g, b + other.b, a + other.a);
		}
		constexpr Image::Color as_color() const
		{
			return Image::Color {
				.r = static_cast<uint8_t>(r < 0.0 ? 0.0 : r > 255.0 ? 255.0 : r),
				.g = static_cast<uint8_t>(g < 0.0 ? 0.0 : g > 255.0 ? 255.0 : g),
				.b = static_cast<uint8_t>(b < 0.0 ? 0.0 : b > 255.0 ? 255.0 : b),
				.a = static_cast<uint8_t>(a < 0.0 ? 0.0 : a > 255.0 ? 255.0 : a),
			};
		}
	};

	BAN::ErrorOr<BAN::UniqPtr<Image>> Image::resize(uint64_t new_width, uint64_t new_height, ResizeAlgorithm algorithm)
	{
		if (!validate_size(new_width, new_height))
			return BAN::Error::from_errno(EOVERFLOW);

		const double ratio_x = (double)width() / new_width;
		const double ratio_y = (double)height() / new_height;

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

				for (uint64_t y = 0; y < new_height; y++)
				{
					for (uint64_t x = 0; x < new_width; x++)
					{
						const double src_x = x * ratio_x;
						const double src_y = y * ratio_y;
#if __enable_sse
						const double weight_x = src_x - floor(src_x);
						const double weight_y = src_y - floor(src_y);
#else
						const double weight_x = src_x - (uint64_t)src_x;
						const double weight_y = src_y - (uint64_t)src_y;
#endif

						const Color avg_t = Color::average(
							get_clamped_color(src_x + 0.0, src_y),
							get_clamped_color(src_x + 1.0, src_y),
							weight_x
						);
						const Color avg_b = Color::average(
							get_clamped_color(src_x + 0.0, src_y + 1.0),
							get_clamped_color(src_x + 0.0, src_y + 1.0),
							weight_x
						);
						bilinear_bitmap[y * new_width + x] = Color::average(avg_t, avg_b, weight_y);
					}
				}

				return TRY(BAN::UniqPtr<Image>::create(new_width, new_height, BAN::move(bilinear_bitmap)));
			}
			case ResizeAlgorithm::Cubic:
			{
				BAN::Vector<Color> bicubic_bitmap;
				TRY(bicubic_bitmap.resize(new_width * new_height));

				constexpr auto cubic_interpolate =
					[](FloatingColor p[4], double x)
					{
						const auto a = (p[0] * -0.5) + (p[1] *  1.5) + (p[2] * -1.5) + (p[3] *  0.5);
						const auto b =  p[0]         + (p[1] * -2.5) + (p[2] *  2.0) + (p[3] * -0.5);
						const auto c = (p[0] * -0.5)                 + (p[2] *  0.5);
						const auto d =                  p[1];
						return ((a * x + b) * x + c) * x + d;
					};

				for (uint64_t y = 0; y < new_height; y++)
				{
					for (uint64_t x = 0; x < new_width; x++)
					{
						const double src_x = x * ratio_x;
						const double src_y = y * ratio_y;
#if __enable_sse
						const double weight_x = src_x - floor(src_x);
						const double weight_y = src_y - floor(src_y);
#else
						const double weight_x = src_x - (uint64_t)src_x;
						const double weight_y = src_y - (uint64_t)src_y;
#endif

						FloatingColor values[4];
						for (int64_t m = -1; m <= 2; m++)
						{
							FloatingColor p[4];
							p[0] = get_clamped_color(src_x - 1.0, src_y + m);
							p[1] = get_clamped_color(src_x + 0.0, src_y + m);
							p[2] = get_clamped_color(src_x + 1.0, src_y + m);
							p[3] = get_clamped_color(src_x + 2.0, src_y + m);
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
