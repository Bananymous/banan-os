#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H 1

#include <sys/cdefs.h>

__BEGIN_DECLS

#include <stdint.h>

#define BANAN_FB_BPP 32

struct framebuffer_info_t
{
	uint32_t width;
	uint32_t height;
};

__END_DECLS

#endif
