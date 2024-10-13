#include <LibImage/Image.h>

#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>
#include <unistd.h>

void render_to_framebuffer(BAN::UniqPtr<LibImage::Image> image, bool scale)
{
	int fd = open("/dev/fb0", O_RDWR);
	if (fd == -1)
	{
		perror("open");
		exit(1);
	}

	framebuffer_info_t fb_info;
	if (pread(fd, &fb_info, sizeof(fb_info), -1) == -1)
	{
		perror("pread");
		exit(1);
	}

	if (scale)
		image = MUST(image->resize(fb_info.width, fb_info.height));

	ASSERT(BANAN_FB_BPP == 24 || BANAN_FB_BPP == 32);

	size_t mmap_size = fb_info.height * fb_info.width * BANAN_FB_BPP / 8;

	void* mmap_addr = mmap(nullptr, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mmap_addr == MAP_FAILED)
	{
		perror("mmap");
		exit(1);
	}

	uint8_t* u8_fb = reinterpret_cast<uint8_t*>(mmap_addr);

	const auto& bitmap = image->bitmap();
	for (uint64_t y = 0; y < BAN::Math::min<uint64_t>(image->height(), fb_info.height); y++)
	{
		for (uint64_t x = 0; x < BAN::Math::min<uint64_t>(image->width(), fb_info.width); x++)
		{
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 0] = bitmap[y * image->width() + x].b;
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 1] = bitmap[y * image->width() + x].g;
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 2] = bitmap[y * image->width() + x].r;
			if constexpr(BANAN_FB_BPP == 32)
				u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 3] = bitmap[y * image->width() + x].a;
		}
	}

	if (msync(mmap_addr, mmap_size, MS_SYNC) == -1)
	{
		perror("msync");
		exit(1);
	}

	munmap(mmap_addr, mmap_size);
	close(fd);
}

int usage(char* arg0, int ret)
{
	FILE* out = (ret == 0) ? stdout : stderr;
	fprintf(out, "usage: %s [options]... IMAGE_PATH\n", arg0);
	fprintf(out, "options:\n");
	fprintf(out, "    -h, --help:   show this message and exit\n");
	fprintf(out, "    -s, --scale:  scale image to framebuffer size\n");
	return ret;
}

int main(int argc, char** argv)
{
	if (argc < 2)
		return usage(argv[0], 1);

	bool scale = false;
	bool benchmark = false;
	for (int i = 1; i < argc - 1; i++)
	{
		if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--scale") == 0)
			scale = true;
		else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--benchmark") == 0)
			benchmark = true;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			return usage(argv[0], 0);
		else
			return usage(argv[0], 1);
	}

	auto image_path = BAN::StringView(argv[argc - 1]);

	timespec load_start, load_end;
	clock_gettime(CLOCK_MONOTONIC, &load_start);
	auto image_or_error = LibImage::Image::load_from_file(image_path);
	clock_gettime(CLOCK_MONOTONIC, &load_end);

	if (image_or_error.is_error())
	{
		fprintf(stderr, "Could not load image '%.*s': %s\n",
			(int)image_path.size(),
			image_path.data(),
			strerror(image_or_error.error().get_error_code())
		);
		return 1;
	}

	if (benchmark)
	{
		const uint64_t start_ms = load_start.tv_sec * 1000 + load_start.tv_nsec / 1'000'000;
		const uint64_t end_ms   =   load_end.tv_sec * 1000 +   load_end.tv_nsec / 1'000'000;
		const uint64_t duration_ms = end_ms - start_ms;
		printf("image load took %" PRIu64 ".%03" PRIu64 " s\n", duration_ms / 1000, duration_ms % 1000);

		if (scale)
		{
			timespec scale_start, scale_end;

			clock_gettime(CLOCK_MONOTONIC, &scale_start);
			auto scaled = MUST(image_or_error.value()->resize(1920, 1080, LibImage::Image::ResizeAlgorithm::Linear));
			clock_gettime(CLOCK_MONOTONIC, &scale_end);

			const uint64_t start_ms = scale_start.tv_sec * 1000 + scale_start.tv_nsec / 1'000'000;
			const uint64_t end_ms   =   scale_end.tv_sec * 1000 +   scale_end.tv_nsec / 1'000'000;
			const uint64_t duration_ms = end_ms - start_ms;
			printf("image scale (%" PRIu64 "x%" PRIu64 " to %dx%d) took %" PRIu64 ".%03" PRIu64 " s\n",
				image_or_error.value()->width(), image_or_error.value()->height(),
				1920, 1080,
				duration_ms / 1000, duration_ms % 1000
			);
		}

		return 0;
	}

	render_to_framebuffer(image_or_error.release_value(), scale);

	for (;;)
		sleep(1);

	return 0;
}
