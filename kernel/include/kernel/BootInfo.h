#pragma once

#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/RSDP.h>

namespace Kernel
{

	struct FramebufferInfo
	{
		enum class Type
		{
			None,
			Unknown,
			RGB,
			Text,
		};

		paddr_t  address;
		uint32_t pitch;
		uint32_t width;
		uint32_t height;
		uint8_t  bpp;
		Type     type;
	};

	struct MemoryMapEntry
	{
		enum class Type
		{
			Available,
			Reserved,
			ACPIReclaim,
			ACPINVS,
		};

		paddr_t  address;
		uint64_t length;
		Type     type;
	};

	struct BootInfo
	{
		BAN::String     command_line;
		FramebufferInfo framebuffer  {};
		RSDP            rsdp         {};
		paddr_t         kernel_paddr {};

		BAN::Vector<MemoryMapEntry>	memory_map_entries;
	};

	bool validate_boot_magic(uint32_t magic);
	void parse_boot_info(uint32_t magic, uint32_t info);
	BAN::StringView get_early_boot_command_line(uint32_t magic, uint32_t info);

	extern BootInfo g_boot_info;
	extern bool g_disable_disk_write;

}
