#pragma once

#include <BAN/String.h>
#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/RSDP.h>

namespace Kernel
{

	enum class FramebufferType
	{
		NONE,
		UNKNOWN,
		RGB
	};

	struct FramebufferInfo
	{
		paddr_t			address;
		uint32_t		pitch;
		uint32_t		width;
		uint32_t		height;
		uint8_t			bpp;
		FramebufferType type = FramebufferType::NONE;
	};

	struct MemoryMapEntry
	{
		uint32_t	type;
		paddr_t		address;
		uint64_t	length;
	};

	struct BootInfo
	{
		BAN::String					command_line;
		FramebufferInfo				framebuffer			{};
		RSDP						rsdp				{};
		BAN::Vector<MemoryMapEntry>	memory_map_entries;
	};

	bool validate_boot_magic(uint32_t magic);
	void parse_boot_info(uint32_t magic, uint32_t info);
	BAN::StringView get_early_boot_command_line(uint32_t magic, uint32_t info);

	extern BootInfo g_boot_info;

}
