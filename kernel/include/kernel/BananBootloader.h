#pragma once

#include <stdint.h>

#define BANAN_BOOTLOADER_MAGIC	0xD3C60CFF
#define BANAN_BOOTLOADER_FB_RGB	1

struct BananBootFramebufferInfo
{
	uint32_t address;
	uint32_t pitch;
	uint32_t width;
	uint32_t height;
	uint8_t bpp;
	uint8_t type;
};

struct BananBootloaderMemoryMapEntry
{
	uint64_t address;
	uint64_t length;
	uint32_t type;
} __attribute__((packed));

struct BananBootloaderMemoryMapInfo
{
	uint32_t entry_count;
	struct BananBootloaderMemoryMapEntry entries[];
} __attribute__((packed));

struct BananBootloaderInfo
{
	uint32_t command_line_addr;
	uint32_t framebuffer_addr;
	uint32_t memory_map_addr;
	uint32_t kernel_paddr;
} __attribute__((packed));
