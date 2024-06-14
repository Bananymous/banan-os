#include <LibImage/Image.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>
#include <unistd.h>

void render_to_framebuffer(const LibImage::Image& image)
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

	ASSERT(BANAN_FB_BPP == 24 || BANAN_FB_BPP == 32);

	size_t mmap_size = fb_info.height * fb_info.width * BANAN_FB_BPP / 8;

	void* mmap_addr = mmap(nullptr, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mmap_addr == MAP_FAILED)
	{
		perror("mmap");
		exit(1);
	}

	uint8_t* u8_fb = reinterpret_cast<uint8_t*>(mmap_addr);

	const auto& bitmap = image.bitmap();
	for (uint64_t y = 0; y < BAN::Math::min<uint64_t>(image.height(), fb_info.height); y++)
	{
		for (uint64_t x = 0; x < BAN::Math::min<uint64_t>(image.width(), fb_info.width); x++)
		{
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 0] = bitmap[y * image.width() + x].r;
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 1] = bitmap[y * image.width() + x].g;
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 2] = bitmap[y * image.width() + x].b;
			if constexpr(BANAN_FB_BPP == 32)
				u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 3] = bitmap[y * image.width() + x].a;
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
	fprintf(out, "usage: %s IMAGE_PATH\n", arg0);
	return ret;
}

int main(int argc, char** argv)
{
	if (argc != 2)
		return usage(argv[0], 1);

	auto image_or_error = LibImage::Image::load_from_file(argv[1]);
	if (image_or_error.is_error())
		return 1;
	auto image = image_or_error.release_value();
	ASSERT(image);

	render_to_framebuffer(*image);

	for (;;)
		sleep(1);

	return 0;
}
