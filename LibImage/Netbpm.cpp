#include <BAN/Optional.h>

#include <LibImage/Netbpm.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

namespace LibImage
{

	static BAN::Optional<uint64_t> parse_u64(BAN::ConstByteSpan& data)
	{
		size_t digit_count = 0;
		while (digit_count < data.size() && isdigit(data[digit_count]))
			digit_count++;
		if (digit_count == 0)
			return {};

		uint64_t result = 0;
		for (size_t i = 0; i < digit_count; i++)
		{
			if (BAN::Math::will_multiplication_overflow<uint64_t>(result, 10))
				return {};
			result *= 10;

			if (BAN::Math::will_addition_overflow<uint64_t>(result, data[i] - '0'))
				return {};
			result += data[i] - '0';
		}

		data = data.slice(digit_count);

		return result;
	}

	BAN::ErrorOr<BAN::UniqPtr<Image>> load_netbpm(BAN::ConstByteSpan image_data)
	{
		if (image_data.size() < 11)
		{
			fprintf(stddbg, "invalid Netbpm image (too small)\n");
			return BAN::Error::from_errno(EINVAL);
		}

		if (image_data[0] != 'P')
		{
			fprintf(stddbg, "not Netbpm image\n");
			return BAN::Error::from_errno(EINVAL);
		}
		if (image_data[1] != '6')
		{
			fprintf(stddbg, "unsupported Netbpm image\n");
			return BAN::Error::from_errno(EINVAL);
		}
		if (image_data[2] != '\n')
		{
			fprintf(stddbg, "invalid Netbpm image (invalid header)\n");
			return BAN::Error::from_errno(EINVAL);
		}
		image_data = image_data.slice(3);

		auto opt_width = parse_u64(image_data);
		if (!opt_width.has_value() || image_data[0] != ' ')
		{
			fprintf(stddbg, "invalid Netbpm image (invalid width)\n");
			return BAN::Error::from_errno(EINVAL);
		}
		image_data = image_data.slice(1);
		auto width = opt_width.value();

		auto opt_height = parse_u64(image_data);
		if (!opt_height.has_value() || image_data[0] != '\n')
		{
			fprintf(stddbg, "invalid Netbpm image (invalid height)\n");
			return BAN::Error::from_errno(EINVAL);
		}
		image_data = image_data.slice(1);
		auto height = opt_height.value();

		if (BAN::Math::will_multiplication_overflow<uint64_t>(width, height) || BAN::Math::will_multiplication_overflow<uint64_t>(width * height, 3))
		{
			fprintf(stddbg, "invalid Netbpm image (size is over 64 bits overflows)\n");
			return BAN::Error::from_errno(EINVAL);
		}

		auto header_end = parse_u64(image_data);
		if (!header_end.has_value() || *header_end != 255 || image_data[0] != '\n')
		{
			fprintf(stddbg, "invalid Netbpm image (invalid header end)\n");
			return BAN::Error::from_errno(EINVAL);
		}
		image_data = image_data.slice(1);

		if (image_data.size() < width * height * 3)
		{
			fprintf(stddbg, "invalid Netbpm image (too small file size)\n");
			return BAN::Error::from_errno(EINVAL);
		}

		BAN::Vector<Image::Color> bitmap;
		TRY(bitmap.resize(width * height));

		// Fill bitmap
		for (uint64_t y = 0; y < height; y++)
		{
			for (uint64_t x = 0; x < width; x++)
			{
				const uint64_t index = y * width + x;
				auto& pixel = bitmap[index];
				pixel.r = image_data[index * 3 + 0];
				pixel.g = image_data[index * 3 + 1];
				pixel.b = image_data[index * 3 + 2];
				pixel.a = 0xFF;
			}
		}

		return TRY(BAN::UniqPtr<Image>::create(width, height, BAN::move(bitmap)));
	}

}
