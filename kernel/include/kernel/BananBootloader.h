#pragma once

#include <stdint.h>

#define BANAN_BOOTLOADER_MAGIC 0xD3C60CFF

struct BananBootloaderMemoryMapEntry
{
	uint64_t address;
	uint64_t length;
	uint32_t type;
} __attribute__((packed));

struct BananBootloaderMemoryMapInfo
{
	uint32_t						entry_count;
	BananBootloaderMemoryMapEntry	entries[];
} __attribute__((packed));

struct BananBootloaderInfo
{
	uint32_t command_line_addr;
	uint32_t memory_map_addr;
} __attribute__((packed));
