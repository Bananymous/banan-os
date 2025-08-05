#pragma once

#include <stdint.h>

#define MULTIBOOT_MAGIC 0x2badb002

#define MULTIBOOT_FLAGS_MEM         (1 << 0)
#define MULTIBOOT_FLAGS_BOOT_DEV    (1 << 1)
#define MULTIBOOT_FLAGS_CMDLINE     (1 << 2)
#define MULTIBOOT_FLAGS_MODULES     (1 << 3)
#define MULTIBOOT_FLAGS_SYMS1       (1 << 4)
#define MULTIBOOT_FLAGS_SYMS2       (1 << 5)
#define MULTIBOOT_FLAGS_MMAP        (1 << 6)
#define MULTIBOOT_FLAGS_DRIVES      (1 << 7)
#define MULTIBOOT_FLAGS_CONFIG      (1 << 8)
#define MULTIBOOT_FLAGS_BOOTLOADER  (1 << 9)
#define MULTIBOOT_FLAGS_APM_TABLE   (1 << 10)
#define MULTIBOOT_FLAGS_VBE         (1 << 11)
#define MULTIBOOT_FLAGS_FRAMEBUFFER (1 << 12)

#define MULTIBOOT_FRAMEBUFFER_TYPE_RGB  1
#define MULTIBOOT_FRAMEBUFFER_TYPE_TEXT 2

struct multiboot_info_t
{
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t syms[4];
	uint32_t mmap_length;
	uint32_t mmap_addr;
	uint32_t drives_length;
	uint32_t drives_addr;
	uint32_t config_table;
	uint32_t bootloader_name;
	uint32_t apm_table;
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint16_t vbe_mode;
	uint16_t vbe_interface_seg;
	uint16_t vbe_interface_off;
	uint16_t vbe_interface_len;
	uint64_t framebuffer_addr;
	uint32_t framebuffer_pitch;
	uint32_t framebuffer_width;
	uint32_t framebuffer_height;
	uint8_t framebuffer_bpp;
	uint8_t framebuffer_type;
	uint8_t color_info[6];
} __attribute__((packed));

struct multiboot_module_t
{
	uint32_t mod_start;
	uint32_t mod_end;
	uint32_t string;
	uint32_t reserved;
} __attribute__((packed));

struct multiboot_mmap_t
{
	uint32_t size;
	uint64_t base_addr;
	uint64_t length;
	uint32_t type;
} __attribute__((packed));
