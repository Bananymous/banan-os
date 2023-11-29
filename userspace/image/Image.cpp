#include "Image.h"
#include "Netbpm.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>

BAN::UniqPtr<Image> Image::load_from_file(BAN::StringView path)
{
	int fd = -1;

	if (path.data()[path.size()] == '\0')
	{
		fd = open(path.data(), O_RDONLY);
	}
	else
	{
		char* buffer = (char*)malloc(path.size() + 1);
		if (!buffer)
		{
			perror("malloc");
			return {};
		}
		memcpy(buffer, path.data(), path.size());
		buffer[path.size()] = '\0';

		fd = open(path.data(), O_RDONLY);

		free(buffer);
	}

	if (fd == -1)
	{
		perror("open");
		return {};
	}

	struct stat st;
	if (fstat(fd, &st) == -1)
	{
		perror("fstat");
		close(fd);
		return {};
	}

	if (st.st_size < 2)
	{
		fprintf(stderr, "invalid image (too small)\n");
		close(fd);
		return {};
	}

	void* addr = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
	{
		perror("mmap");
		close(fd);
		return {};
	}

	BAN::UniqPtr<Image> image;

	uint16_t u16_signature = *reinterpret_cast<uint16_t*>(addr);
	switch (u16_signature)
	{
		case 0x3650:
		case 0x3550:
		case 0x3450:
		case 0x3350:
		case 0x3250:
		case 0x3150:
			if (auto res = Netbpm::create(addr, st.st_size); res.is_error())
				fprintf(stderr, "%s\n", strerror(res.error().get_error_code()));
			else
				image = res.release_value();
			break;
		default:
			fprintf(stderr, "unrecognized image format\n");
			break;
	}

	munmap(addr, st.st_size);
	close(fd);

	return image;
}

bool Image::render_to_framebuffer()
{
	int fd = open("/dev/fb0", O_RDWR);
	if (fd == -1)
	{
		perror("open");
		return false;
	}

	framebuffer_info_t fb_info;
	if (pread(fd, &fb_info, sizeof(fb_info), -1) == -1)
	{
		perror("pread");
		close(fd);
		return false;
	}

	ASSERT(BANAN_FB_BPP == 24 || BANAN_FB_BPP == 32);

	size_t mmap_size = fb_info.height * fb_info.width * BANAN_FB_BPP / 8;

	void* mmap_addr = mmap(nullptr, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mmap_addr == MAP_FAILED)
	{
		perror("mmap");
		close(fd);
		return false;
	}

	uint8_t* u8_fb = reinterpret_cast<uint8_t*>(mmap_addr);

	for (uint64_t y = 0; y < BAN::Math::min<uint64_t>(height(), fb_info.height); y++)
	{
		for (uint64_t x = 0; x < BAN::Math::min<uint64_t>(width(), fb_info.width); x++)
		{
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 0] = m_bitmap[y * width() + x].r;
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 1] = m_bitmap[y * width() + x].g;
			u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 2] = m_bitmap[y * width() + x].b;
			if constexpr(BANAN_FB_BPP == 32)
				u8_fb[(y * fb_info.width + x) * BANAN_FB_BPP / 8 + 3] = m_bitmap[y * width() + x].a;
		}
	}

	if (msync(mmap_addr, mmap_size, MS_SYNC) == -1)
	{
		perror("msync");
		munmap(mmap_addr, mmap_size);
		close(fd);
		return false;
	}

	munmap(mmap_addr, mmap_size);
	close(fd);

	return true;
}
