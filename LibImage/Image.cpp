#include <BAN/ScopeGuard.h>
#include <BAN/String.h>

#include <LibImage/Image.h>
#include <LibImage/Netbpm.h>

#include <fcntl.h>
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

		if (st.st_size < 2)
		{
			fprintf(stddbg, "invalid image (too small)\n");
			return BAN::Error::from_errno(EINVAL);
		}

		void* addr = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (addr == MAP_FAILED)
		{
			fprintf(stddbg, "mmap: %s\n", strerror(errno));
			return BAN::Error::from_errno(errno);
		}

		BAN::ScopeGuard guard_munmap([&] { munmap(addr, st.st_size); });

		auto image_data_span = BAN::ConstByteSpan(reinterpret_cast<uint8_t*>(addr), st.st_size);

		uint16_t u16_signature = image_data_span.as<const uint16_t>();
		switch (u16_signature)
		{
			case 0x3650:
			case 0x3550:
			case 0x3450:
			case 0x3350:
			case 0x3250:
			case 0x3150:
				return TRY(load_netbpm(image_data_span));
			default:
				fprintf(stderr, "unrecognized image format\n");
				break;
		}

		return BAN::Error::from_errno(ENOTSUP);
	}

	BAN::ErrorOr<BAN::UniqPtr<Image>> Image::resize(uint64_t new_width, uint64_t new_height, ResizeAlgorithm algorithm)
	{
		if (!validate_size(new_width, new_height))
			return BAN::Error::from_errno(EOVERFLOW);

		const double ratio_x = (double)width() / new_width;
		const double ratio_y = (double)height() / new_height;

		switch (algorithm)
		{
			case ResizeAlgorithm::Nearest:
			{
				BAN::Vector<Color> nearest_bitmap;
				TRY(nearest_bitmap.resize(new_width * new_height));

				for (uint64_t y = 0; y < new_height; y++)
				{
					for (uint64_t x = 0; x < new_width; x++)
					{
						const uint64_t nearest_x = BAN::Math::clamp<uint64_t>(x * ratio_x, 0, width() - 1);
						const uint64_t nearest_y = BAN::Math::clamp<uint64_t>(y * ratio_y, 0, height() - 1);
						nearest_bitmap[y * new_width + x] = get_color(nearest_x, nearest_y);
					}
				}

				return TRY(BAN::UniqPtr<Image>::create(new_width, new_height, BAN::move(nearest_bitmap)));
			}
			case ResizeAlgorithm::Bilinear:
			{
				BAN::Vector<Color> bilinear_bitmap;
				TRY(bilinear_bitmap.resize(new_width * new_height));

				for (uint64_t y = 0; y < new_height; y++)
				{
					for (uint64_t x = 0; x < new_width; x++)
					{
						const double src_x_float = x * ratio_x;
						const double src_y_float = y * ratio_y;
						const double weight_x = src_x_float - floor(src_x_float);
						const double weight_y = src_y_float - floor(src_y_float);

						const uint64_t src_l = BAN::Math::clamp<uint64_t>(src_x_float, 0, width() - 1);
						const uint64_t src_t = BAN::Math::clamp<uint64_t>(src_y_float, 0, height() - 1);

						const uint64_t src_r = BAN::Math::clamp<uint64_t>(src_l + 1, 0, width() - 1);
						const uint64_t src_b = BAN::Math::clamp<uint64_t>(src_t + 1, 0, height() - 1);

						const Color avg_t = Color::average(get_color(src_l, src_t), get_color(src_r, src_t), weight_x);
						const Color avg_b = Color::average(get_color(src_l, src_b), get_color(src_r, src_b), weight_x);
						bilinear_bitmap[y * new_width + x] = Color::average(avg_t, avg_b, weight_y);
					}
				}

				return TRY(BAN::UniqPtr<Image>::create(new_width, new_height, BAN::move(bilinear_bitmap)));
			}
		}

		return BAN::Error::from_errno(EINVAL);
	}

}
