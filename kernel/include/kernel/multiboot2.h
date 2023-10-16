#pragma once

#include <stdint.h>

// https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#Boot-information

#define MULTIBOOT2_TAG_END			0
#define MULTIBOOT2_TAG_CMDLINE		1
#define MULTIBOOT2_TAG_MMAP			6
#define MULTIBOOT2_TAG_FRAMEBUFFER	8
#define MULTIBOOT2_TAG_OLD_RSDP		14
#define MULTIBOOT2_TAG_NEW_RSDP		15

#define MULTIBOOT2_FRAMEBUFFER_TYPE_RGB	1

struct multiboot2_tag_t
{
	uint32_t type;
	uint32_t size;
	multiboot2_tag_t* next() { return (multiboot2_tag_t*)((uintptr_t)this + ((size + 7) & ~7)); }
} __attribute__((packed));

struct multiboot2_cmdline_tag_t : public multiboot2_tag_t
{
	char cmdline[];
} __attribute__((packed));

struct multiboot2_mmap_entry_t
{
	uint64_t base_addr;
	uint64_t length;
	uint32_t type;
	uint32_t reserved;
} __attribute__((packed));

struct multiboot2_mmap_tag_t : public multiboot2_tag_t
{
	uint32_t entry_size;
	uint32_t entry_version;
	multiboot2_mmap_entry_t entries[];
} __attribute__((packed));

struct multiboot2_framebuffer_tag_t : public multiboot2_tag_t
{
	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t framebuffer_bpp;
	uint8_t framebuffer_type;
	uint8_t reserved;
} __attribute__((packed));

struct multiboot2_rsdp_tag_t : public multiboot2_tag_t
{
	uint8_t data[];
} __attribute__((packed));

struct multiboot2_info_t
{
	uint32_t total_size;
	uint32_t reserved;
	multiboot2_tag_t tags[];
} __attribute__((packed));

extern "C" multiboot2_info_t* g_multiboot2_info;
extern "C" uint32_t g_multiboot2_magic;

inline multiboot2_tag_t* multiboot2_find_tag(uint32_t type)
{
	for (auto* tag = g_multiboot2_info->tags; tag->type != MULTIBOOT2_TAG_END; tag = tag->next())
		if (tag->type == type)
			return tag;
	return nullptr;
}
