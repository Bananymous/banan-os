#include "Netbpm.h"

#include <BAN/Optional.h>

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

BAN::Optional<uint64_t> parse_u64(const uint8_t*& data, size_t data_size)
{
	uint64_t result = 0;

	// max supported size 10^20 - 1
	for (size_t i = 0; i < 19; i++)
	{
		if (i >= data_size)
		{
			if (isdigit(*data))
				return {};
			return result;
		}

		if (!isdigit(*data))
			return result;

		result = (result * 10) + (*data - '0');
		data++;
	}

	return {};
}

BAN::ErrorOr<BAN::UniqPtr<Image>> load_netbpm(const void* mmap_addr, size_t size)
{
	if (size < 11)
	{
		fprintf(stderr, "invalid Netbpm image (too small)\n");
		return BAN::Error::from_errno(EINVAL);
	}

	const uint8_t* u8_ptr = reinterpret_cast<const uint8_t*>(mmap_addr);
	
	if (u8_ptr[0] != 'P')
	{
		fprintf(stderr, "not Netbpm image\n");
		return BAN::Error::from_errno(EINVAL);
	}
	if (u8_ptr[1] != '6')
	{
		fprintf(stderr, "unsupported Netbpm image\n");
		return BAN::Error::from_errno(EINVAL);
	}
	if (u8_ptr[2] != '\n')
	{
		fprintf(stderr, "invalid Netbpm image (invalid header)\n");
		return BAN::Error::from_errno(EINVAL);
	}
	u8_ptr += 3;

	auto width = parse_u64(u8_ptr, size - (u8_ptr - reinterpret_cast<const uint8_t*>(mmap_addr)));
	if (!width.has_value() || *u8_ptr != ' ')
	{
		fprintf(stderr, "invalid Netbpm image (invalid width)\n");
		return BAN::Error::from_errno(EINVAL);
	}
	u8_ptr++;

	auto height = parse_u64(u8_ptr, size - (u8_ptr - reinterpret_cast<const uint8_t*>(mmap_addr)));
	if (!height.has_value() || *u8_ptr != '\n')
	{
		fprintf(stderr, "invalid Netbpm image (invalid height)\n");
		return BAN::Error::from_errno(EINVAL);
	}
	u8_ptr++;

	auto header_end = parse_u64(u8_ptr, size - (u8_ptr - reinterpret_cast<const uint8_t*>(mmap_addr)));
	if (!header_end.has_value() || *header_end != 255 || *u8_ptr != '\n')
	{
		fprintf(stderr, "invalid Netbpm image (invalid header end)\n");
		return BAN::Error::from_errno(EINVAL);
	}
	u8_ptr++;

	if (size - (u8_ptr - reinterpret_cast<const uint8_t*>(mmap_addr)) < *width * *height * 3)
	{
		fprintf(stderr, "invalid Netbpm image (too small file size)\n");
		return BAN::Error::from_errno(EINVAL);
	}

	printf("Netbpm image %" PRIuPTR "x%" PRIuPTR "\n", *width, *height);

	BAN::Vector<Image::Color> bitmap;
	TRY(bitmap.resize(*width * *height));

	// Fill bitmap
	for (uint64_t y = 0; y < *height; y++)
	{
		for (uint64_t x = 0; x < *width; x++)
		{
			auto& pixel = bitmap[y * *width + x];
			pixel.r = *u8_ptr++;
			pixel.g = *u8_ptr++;
			pixel.b = *u8_ptr++;
			pixel.a = 0xFF;
		}
	}

	return TRY(BAN::UniqPtr<Image>::create(*width, *height, BAN::move(bitmap)));
}
