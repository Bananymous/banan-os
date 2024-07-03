#include "Framebuffer.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>

Framebuffer open_framebuffer()
{
	int framebuffer_fd = open("/dev/fb0", O_RDWR);
	if (framebuffer_fd == -1)
	{
		perror("open");
		exit(1);
	}

	framebuffer_info_t framebuffer_info;
	if (pread(framebuffer_fd, &framebuffer_info, sizeof(framebuffer_info), -1) == -1)
	{
		perror("pread");
		exit(1);
	}

	const size_t framebuffer_bytes = framebuffer_info.width * framebuffer_info.height * (BANAN_FB_BPP / 8);

	uint32_t* framebuffer_mmap = (uint32_t*)mmap(NULL, framebuffer_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, framebuffer_fd, 0);
	if (framebuffer_mmap == MAP_FAILED)
	{
		perror("mmap");
		exit(1);
	}

	memset(framebuffer_mmap, 0, framebuffer_bytes);
	msync(framebuffer_mmap, framebuffer_bytes, MS_SYNC);

	Framebuffer framebuffer;
	framebuffer.fd = framebuffer_fd;
	framebuffer.mmap = framebuffer_mmap;
	framebuffer.width = framebuffer_info.width;
	framebuffer.height = framebuffer_info.height;
	framebuffer.bpp = BANAN_FB_BPP;
	return framebuffer;
}
