#include <fcntl.h>
#include <string.h>
#include <sys/framebuffer.h>
#include <sys/mman.h>

int main()
{
	int fd = open("/dev/fb0", O_RDWR);
	if (fd == -1)
	{
		perror("open");
		return 1;
	}

	framebuffer_info_t fb_info;
	if (pread(fd, &fb_info, sizeof(fb_info), -1) == -1)
	{
		perror("read");
		return 1;
	}

	size_t fb_size = fb_info.width * fb_info.height * fb_info.bpp / 8;

	void* addr = mmap(nullptr, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED)
	{
		perror("mmap");
		return 1;
	}

	memset(addr, 0xFF, fb_size);

	if (msync(addr, fb_size, MS_SYNC) == -1)
	{
		perror("msync");
		return 1;
	}

	sleep(4);

	munmap(addr, fb_size);
	close(fd);

	return 0;
}
