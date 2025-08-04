#include <kernel/BootInfo.h>
#include <kernel/BananBootloader.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>

namespace Kernel
{

	BootInfo g_boot_info;
	bool g_disable_disk_write { false };

	static MemoryMapEntry::Type bios_number_to_memory_type(uint32_t number)
	{
		switch (number)
		{
			case 1: return MemoryMapEntry::Type::Available;
			case 2: return MemoryMapEntry::Type::Reserved;
			case 3: return MemoryMapEntry::Type::ACPIReclaim;
			case 4: return MemoryMapEntry::Type::ACPINVS;
		}
		return MemoryMapEntry::Type::Reserved;
	}

	static void parse_boot_info_multiboot(uint32_t info)
	{
		const auto& multiboot_info = *reinterpret_cast<const multiboot_info_t*>(info);

		if (multiboot_info.flags & MULTIBOOT_FLAGS_CMDLINE)
		{
			MUST(g_boot_info.command_line.append(reinterpret_cast<const char*>(multiboot_info.cmdline)));
		}

		if (multiboot_info.flags & MULTIBOOT_FLAGS_MODULES)
		{
			for (size_t i = 0; i < multiboot_info.mods_count; i++)
			{
				const auto& module = reinterpret_cast<const multiboot_module_t*>(multiboot_info.mods_addr)[i];
				MUST(g_boot_info.modules.emplace_back(module.mod_start, module.mod_end - module.mod_start));
			}
		}

		if (multiboot_info.flags & MULTIBOOT_FLAGS_MMAP)
		{
			uintptr_t address = multiboot_info.mmap_addr;
			while (address < multiboot_info.mmap_addr + multiboot_info.mmap_length)
			{
				const auto& mmap_entry = *reinterpret_cast<const multiboot_mmap_t*>(address);
				dprintln("entry {16H} {16H} {8H}",
					(uint64_t)mmap_entry.base_addr,
					(uint64_t)mmap_entry.length,
					(uint64_t)mmap_entry.type
				);
				MUST(g_boot_info.memory_map_entries.push_back({
					.address = mmap_entry.base_addr,
					.length  = mmap_entry.length,
					.type    = bios_number_to_memory_type(mmap_entry.type),
				}));
				address += mmap_entry.size + sizeof(mmap_entry.size);
			}
		}

		if (multiboot_info.flags & MULTIBOOT_FLAGS_FRAMEBUFFER)
		{
			g_boot_info.framebuffer.address	= multiboot_info.framebuffer_addr;
			g_boot_info.framebuffer.pitch	= multiboot_info.framebuffer_pitch;
			g_boot_info.framebuffer.width	= multiboot_info.framebuffer_width;
			g_boot_info.framebuffer.height	= multiboot_info.framebuffer_height;
			g_boot_info.framebuffer.bpp		= multiboot_info.framebuffer_bpp;
			if (multiboot_info.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
				g_boot_info.framebuffer.type = FramebufferInfo::Type::RGB;
			else if (multiboot_info.framebuffer_type == MULTIBOOT_FRAMEBUFFER_TYPE_TEXT)
				g_boot_info.framebuffer.type = FramebufferInfo::Type::Text;
			else
				g_boot_info.framebuffer.type = FramebufferInfo::Type::Unknown;
		}

		g_boot_info.kernel_paddr = 0;
	}

	static BAN::StringView get_early_boot_command_line_multiboot(uint32_t info)
	{
		const auto& multiboot_info = *reinterpret_cast<const multiboot_info_t*>(info);
		if (!(multiboot_info.flags & MULTIBOOT_FLAGS_CMDLINE))
			return ""_sv;
		return BAN::StringView(reinterpret_cast<const char*>(multiboot_info.cmdline));
	}

	static void parse_boot_info_multiboot2(uint32_t info)
	{
		const auto& multiboot2_info = *reinterpret_cast<const multiboot2_info_t*>(info);

		for (const auto* tag = multiboot2_info.tags; tag->type != MULTIBOOT2_TAG_END; tag = tag->next())
		{
			switch (tag->type)
			{
				case MULTIBOOT2_TAG_CMDLINE:
				{
					const auto& command_line_tag = *static_cast<const multiboot2_cmdline_tag_t*>(tag);
					MUST(g_boot_info.command_line.append(command_line_tag.cmdline));
					break;
				}
				case MULTIBOOT2_TAG_MODULES:
				{
					const auto& modules_tag = *static_cast<const multiboot2_modules_tag_t*>(tag);
					MUST(g_boot_info.modules.emplace_back(modules_tag.mod_start, modules_tag.mod_end - modules_tag.mod_start));
					break;
				}
				case MULTIBOOT2_TAG_FRAMEBUFFER:
				{
					const auto& framebuffer_tag = *static_cast<const multiboot2_framebuffer_tag_t*>(tag);
					g_boot_info.framebuffer.address	= framebuffer_tag.framebuffer_addr;
					g_boot_info.framebuffer.pitch	= framebuffer_tag.framebuffer_pitch;
					g_boot_info.framebuffer.width	= framebuffer_tag.framebuffer_width;
					g_boot_info.framebuffer.height	= framebuffer_tag.framebuffer_height;
					g_boot_info.framebuffer.bpp		= framebuffer_tag.framebuffer_bpp;
					if (framebuffer_tag.framebuffer_type == MULTIBOOT2_FRAMEBUFFER_TYPE_RGB)
						g_boot_info.framebuffer.type = FramebufferInfo::Type::RGB;
					else if (framebuffer_tag.framebuffer_type == MULTIBOOT2_FRAMEBUFFER_TYPE_TEXT)
						g_boot_info.framebuffer.type = FramebufferInfo::Type::Text;
					else
						g_boot_info.framebuffer.type = FramebufferInfo::Type::Unknown;
					break;
				}
				case MULTIBOOT2_TAG_MMAP:
				{
					const auto& mmap_tag = *static_cast<const multiboot2_mmap_tag_t*>(tag);

					const size_t entry_count = (mmap_tag.size - sizeof(multiboot2_mmap_tag_t)) / mmap_tag.entry_size;

					MUST(g_boot_info.memory_map_entries.resize(entry_count));

					for (size_t i = 0; i < entry_count; i++)
					{
						const auto& mmap_entry = *reinterpret_cast<const multiboot2_mmap_entry_t*>(reinterpret_cast<uintptr_t>(tag) + sizeof(multiboot2_mmap_tag_t) + i * mmap_tag.entry_size);
						dprintln("entry {16H} {16H} {8H}",
							(uint64_t)mmap_entry.base_addr,
							(uint64_t)mmap_entry.length,
							(uint64_t)mmap_entry.type
						);
						g_boot_info.memory_map_entries[i].address = mmap_entry.base_addr;
						g_boot_info.memory_map_entries[i].length  = mmap_entry.length;
						g_boot_info.memory_map_entries[i].type    = bios_number_to_memory_type(mmap_entry.type);
					}
					break;
				}
				case MULTIBOOT2_TAG_OLD_RSDP:
				{
					if (g_boot_info.rsdp.length == 0)
					{
						memcpy(&g_boot_info.rsdp, static_cast<const multiboot2_rsdp_tag_t*>(tag)->data, 20);
						g_boot_info.rsdp.length = 20;
					}
					break;
				}
				case MULTIBOOT2_TAG_NEW_RSDP:
				{
					const auto& rsdp = *reinterpret_cast<const RSDP*>(static_cast<const multiboot2_rsdp_tag_t*>(tag)->data);
					memcpy(&g_boot_info.rsdp, &rsdp, BAN::Math::min<uint32_t>(rsdp.length, sizeof(g_boot_info.rsdp)));
					break;
				}
			}
		}

		g_boot_info.kernel_paddr = 0;
	}

	static BAN::StringView get_early_boot_command_line_multiboot2(uint32_t info)
	{
		const auto& multiboot2_info = *reinterpret_cast<const multiboot2_info_t*>(info);
		for (const auto* tag = multiboot2_info.tags; tag->type != MULTIBOOT2_TAG_END; tag = tag->next())
			if (tag->type == MULTIBOOT2_TAG_CMDLINE)
				return static_cast<const multiboot2_cmdline_tag_t*>(tag)->cmdline;
		return {};
	}

	static void parse_boot_info_banan_bootloader(uint32_t info)
	{
		const auto& banan_bootloader_info = *reinterpret_cast<const BananBootloaderInfo*>(info);

		const char* command_line = reinterpret_cast<const char*>(banan_bootloader_info.command_line_addr);
		MUST(g_boot_info.command_line.append(command_line));

		const auto& framebuffer = *reinterpret_cast<BananBootFramebufferInfo*>(banan_bootloader_info.framebuffer_addr);
		g_boot_info.framebuffer.address	= framebuffer.address;
		g_boot_info.framebuffer.width	= framebuffer.width;
		g_boot_info.framebuffer.height	= framebuffer.height;
		g_boot_info.framebuffer.pitch	= framebuffer.pitch;
		g_boot_info.framebuffer.bpp		= framebuffer.bpp;
		if (framebuffer.type == BANAN_BOOTLOADER_FB_RGB)
			g_boot_info.framebuffer.type = FramebufferInfo::Type::RGB;
		else if (framebuffer.type == BANAN_BOOTLOADER_FB_TEXT)
			g_boot_info.framebuffer.type = FramebufferInfo::Type::Text;
		else
			g_boot_info.framebuffer.type = FramebufferInfo::Type::Unknown;

		const auto& memory_map =  *reinterpret_cast<BananBootloaderMemoryMapInfo*>(banan_bootloader_info.memory_map_addr);
		MUST(g_boot_info.memory_map_entries.resize(memory_map.entry_count));
		for (size_t i = 0; i < memory_map.entry_count; i++)
		{
			const auto& mmap_entry = memory_map.entries[i];
			g_boot_info.memory_map_entries[i].address = mmap_entry.address;
			g_boot_info.memory_map_entries[i].length  = mmap_entry.length;
			g_boot_info.memory_map_entries[i].type    = bios_number_to_memory_type(mmap_entry.type);
		}

		g_boot_info.kernel_paddr = banan_bootloader_info.kernel_paddr;
	}

	static BAN::StringView get_early_boot_command_line_banan_bootloader(uint32_t info)
	{
		const auto& banan_bootloader_info = *reinterpret_cast<const BananBootloaderInfo*>(info);
		return reinterpret_cast<const char*>(banan_bootloader_info.command_line_addr);
	}

	bool validate_boot_magic(uint32_t magic)
	{
		switch (magic)
		{
			case MULTIBOOT_MAGIC:
			case MULTIBOOT2_MAGIC:
			case BANAN_BOOTLOADER_MAGIC:
				return true;
			default:
				return false;
		}
	}

	void parse_boot_info(uint32_t magic, uint32_t info)
	{
		switch (magic)
		{
			case MULTIBOOT_MAGIC:
				return parse_boot_info_multiboot(info);
			case MULTIBOOT2_MAGIC:
				return parse_boot_info_multiboot2(info);
			case BANAN_BOOTLOADER_MAGIC:
				return parse_boot_info_banan_bootloader(info);
		}
		ASSERT_NOT_REACHED();
	}

	BAN::StringView get_early_boot_command_line(uint32_t magic, uint32_t info)
	{
		switch (magic)
		{
			case MULTIBOOT_MAGIC:
				return get_early_boot_command_line_multiboot(info);
			case MULTIBOOT2_MAGIC:
				return get_early_boot_command_line_multiboot2(info);
			case BANAN_BOOTLOADER_MAGIC:
				return get_early_boot_command_line_banan_bootloader(info);
		}
		ASSERT_NOT_REACHED();
	}

}
