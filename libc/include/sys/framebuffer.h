#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdint.h>

struct framebuffer_info_t
{
	uint32_t width;
	uint32_t height;
	uint8_t bpp;
};

__END_DECLS

#endif
