#pragma once

#include <stdint.h>

// https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html#Boot-information

#define MULTIBOOT2_TAG_END			0
#define MULTIBOOT2_TAG_CMDLINE		1
#define MULTIBOOT2_TAG_MODULES		3
#define MULTIBOOT2_TAG_MMAP			6
#define MULTIBOOT2_TAG_FRAMEBUFFER	8
#define MULTIBOOT2_TAG_OLD_RSDP		14
#define MULTIBOOT2_TAG_NEW_RSDP		15

#define MULTIBOOT2_FRAMEBUFFER_TYPE_RGB		1
#define MULTIBOOT2_FRAMEBUFFER_TYPE_TEXT	2

#define MULTIBOOT2_MAGIC 0x36d76289

struct multiboot2_tag_t
{
	uint32_t type;
	uint32_t size;
	const multiboot2_tag_t* next() const
	{
		return reinterpret_cast<const multiboot2_tag_t*>(
			reinterpret_cast<uintptr_t>(this) + ((size + 7) & ~7)
		);
	}
} __attribute__((packed));

struct multiboot2_cmdline_tag_t : public multiboot2_tag_t
{
	char cmdline[];
} __attribute__((packed));

struct multiboot2_modules_tag_t : public multiboot2_tag_t
{
	uint32_t mod_start;
	uint32_t mod_end;
	uint8_t string[];
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
