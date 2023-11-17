#include <kernel/BootInfo.h>

#include <kernel/multiboot2.h>

namespace Kernel
{

	BootInfo g_boot_info;

	void parse_boot_info_multiboot2(uint32_t info)
	{
		const auto& multiboot2_info = *reinterpret_cast<const multiboot2_info_t*>(info);

		for (const auto* tag = multiboot2_info.tags; tag->type != MULTIBOOT2_TAG_END; tag = tag->next())
		{
			if (tag->type == MULTIBOOT2_TAG_CMDLINE)
			{
				const auto& command_line_tag = *reinterpret_cast<const multiboot2_cmdline_tag_t*>(tag);
				MUST(g_boot_info.command_line.append(command_line_tag.cmdline));
			}
			else if (tag->type == MULTIBOOT2_TAG_FRAMEBUFFER)
			{
				const auto& framebuffer_tag = *reinterpret_cast<const multiboot2_framebuffer_tag_t*>(tag);
				g_boot_info.framebuffer.address	= framebuffer_tag.framebuffer_addr;
				g_boot_info.framebuffer.pitch	= framebuffer_tag.framebuffer_pitch;
				g_boot_info.framebuffer.width	= framebuffer_tag.framebuffer_width;
				g_boot_info.framebuffer.height	= framebuffer_tag.framebuffer_height;
				g_boot_info.framebuffer.bpp		= framebuffer_tag.framebuffer_bpp;
				if (framebuffer_tag.framebuffer_type == MULTIBOOT2_FRAMEBUFFER_TYPE_RGB)
					g_boot_info.framebuffer.type = FramebufferType::RGB;
				else
					g_boot_info.framebuffer.type = FramebufferType::UNKNOWN;
			}
			else if (tag->type == MULTIBOOT2_TAG_MMAP)
			{
				const auto& mmap_tag = *reinterpret_cast<const multiboot2_mmap_tag_t*>(tag);

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
					g_boot_info.memory_map_entries[i].address	= mmap_entry.base_addr;
					g_boot_info.memory_map_entries[i].length	= mmap_entry.length;
					g_boot_info.memory_map_entries[i].type		= mmap_entry.type;
				}
			}
		}
	}

	BAN::StringView get_early_boot_command_line_multiboot2(uint32_t info)
	{
		const auto& multiboot2_info = *reinterpret_cast<const multiboot2_info_t*>(info);
		for (const auto* tag = multiboot2_info.tags; tag->type != MULTIBOOT2_TAG_END; tag = tag->next())
			if (tag->type == MULTIBOOT2_TAG_CMDLINE)
				return reinterpret_cast<const multiboot2_cmdline_tag_t*>(tag)->cmdline;
		return {};
	}

	bool validate_boot_magic(uint32_t magic)
	{
		if (magic == MULTIBOOT2_MAGIC)
			return true;
		return false;	
	}

	void parse_boot_info(uint32_t magic, uint32_t info)
	{
		switch (magic)
		{
			case MULTIBOOT2_MAGIC:
				return parse_boot_info_multiboot2(info);
		}
		ASSERT_NOT_REACHED();
	}

	BAN::StringView get_early_boot_command_line(uint32_t magic, uint32_t info)
	{
		switch (magic)
		{
			case MULTIBOOT2_MAGIC:
				return get_early_boot_command_line_multiboot2(info);
		}
		ASSERT_NOT_REACHED();
	}

}
