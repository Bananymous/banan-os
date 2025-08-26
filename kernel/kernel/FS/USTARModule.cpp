#include <BAN/ScopeGuard.h>
#include <kernel/FS/USTARModule.h>

#include <tar.h>

namespace Kernel
{

	bool is_ustar_boot_module(const BootModule& module)
	{
		if (module.start % PAGE_SIZE)
		{
			dprintln("ignoring non-page-aligned module");
			return false;
		}

		if (module.size < 512)
			return false;

		bool has_ustar_signature;
		PageTable::with_fast_page(module.start, [&] {
			has_ustar_signature = memcmp(PageTable::fast_page_as_ptr(257), "ustar", 5) == 0;
		});

		return has_ustar_signature;
	}

	BAN::ErrorOr<void> unpack_boot_module_into_filesystem(BAN::RefPtr<FileSystem> filesystem, const BootModule& module)
	{
		ASSERT(is_ustar_boot_module(module));

		auto root_inode = filesystem->root_inode();

		uint8_t* temp_page = static_cast<uint8_t*>(kmalloc(PAGE_SIZE));
		if (temp_page == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		BAN::ScopeGuard _([temp_page] { kfree(temp_page); });

		BAN::String next_file_name;
		BAN::String next_link_name;

		size_t offset = 0;
		while (offset + 512 <= module.size)
		{
			size_t file_size = 0;
			mode_t file_mode = 0;
			uid_t file_uid = 0;
			gid_t file_gid = 0;
			uint8_t file_type = 0;
			char file_path[100 + 1 + 155 + 1] {};

			PageTable::with_fast_page((module.start + offset) & PAGE_ADDR_MASK, [&] {
				const size_t page_off = offset % PAGE_SIZE;

				const auto parse_octal =
					[page_off](size_t offset, size_t length) -> size_t
					{
						size_t result = 0;
						for (size_t i = 0; i < length; i++)
						{
							const char ch = PageTable::fast_page_as<char>(page_off + offset + i);
							if (ch == '\0')
								break;
							result = (result * 8) + (ch - '0');
						}
						return result;
					};

				if (memcmp(PageTable::fast_page_as_ptr(page_off + 257), "ustar", 5)) {
					file_size = SIZE_MAX;
					return;
				}

				memcpy(file_path, PageTable::fast_page_as_ptr(page_off + 345), 155);
				const size_t prefix_len = strlen(file_path);
				file_path[prefix_len] = '/';
				memcpy(file_path + prefix_len + 1, PageTable::fast_page_as_ptr(page_off), 100);

				file_mode = parse_octal(100, 8);
				file_uid  = parse_octal(108, 8);
				file_gid  = parse_octal(116, 8);
				file_size = parse_octal(124, 12);
				file_type = PageTable::fast_page_as<char>(page_off + 156);
			});

			if (file_size == SIZE_MAX)
				break;
			if (offset + 512 + file_size > module.size)
				break;

			auto parent_inode = filesystem->root_inode();

			auto file_path_parts = TRY(BAN::StringView(next_file_name.empty() ? file_path : next_file_name.sv()).split('/'));
			for (size_t i = 0; i < file_path_parts.size() - 1; i++)
				parent_inode = TRY(parent_inode->find_inode(file_path_parts[i]));

			switch (file_type)
			{
				case 'L': case 'K': break;
				case REGTYPE:
				case AREGTYPE: file_mode |= Inode::Mode::IFREG; break;
				case LNKTYPE:                                   break;
				case SYMTYPE:  file_mode |= Inode::Mode::IFLNK; break;
				case CHRTYPE:  file_mode |= Inode::Mode::IFCHR; break;
				case BLKTYPE:  file_mode |= Inode::Mode::IFBLK; break;
				case DIRTYPE:  file_mode |= Inode::Mode::IFDIR; break;
				case FIFOTYPE: file_mode |= Inode::Mode::IFIFO; break;
				default:
					panic("unknown file type {}", file_type);
			}

			auto file_name_sv = file_path_parts.back();

			if (file_type == 'L' || file_type == 'K')
			{
				auto& target = (file_type == 'L') ? next_file_name : next_link_name;
				TRY(target.resize(file_size));

				size_t nwritten = 0;
				while (nwritten < file_size)
				{
					const paddr_t paddr = module.start + offset + 512 + nwritten;
					PageTable::with_fast_page(paddr & PAGE_ADDR_MASK, [&] {
						memcpy(temp_page, PageTable::fast_page_as_ptr(), PAGE_SIZE);
					});

					const size_t page_off = paddr % PAGE_SIZE;
					const size_t to_write = BAN::Math::min(file_size - nwritten, PAGE_SIZE - page_off);
					memcpy(target.data() + nwritten, temp_page + page_off, to_write);
					nwritten += to_write;
				}

				while (!target.empty() && target.back() == '\0')
					target.pop_back();
			}
			else if (file_type == DIRTYPE)
			{
				if (file_name_sv == "."_sv)
					; // NOTE: don't create "." (root)
				else if (auto ret = parent_inode->create_directory(file_name_sv, file_mode, file_uid, file_gid); ret.is_error())
					dwarnln("failed to create directory '{}': {}", file_name_sv, ret.error());
			}
			else if (file_type == LNKTYPE)
			{
				BAN::StringView link_name;

				char link_buffer[101] {};
				if (!next_link_name.empty())
					link_name = next_link_name.sv();
				else
				{
					const paddr_t paddr = module.start + offset;
					PageTable::with_fast_page(paddr & PAGE_ADDR_MASK, [&] {
						memcpy(link_buffer, PageTable::fast_page_as_ptr((paddr % PAGE_SIZE) + 157), 100);
					});
					link_name = link_buffer;
				}

				auto target_inode = filesystem->root_inode();

				auto link_path_parts = TRY(link_name.split('/'));
				for (const auto part : link_path_parts)
				{
					auto find_result = target_inode->find_inode(part);
					if (!find_result.is_error())
						target_inode = find_result.release_value();
					else
					{
						target_inode = {};
						break;
					}
				}

				if (target_inode)
					if (auto ret = parent_inode->link_inode(file_name_sv, target_inode); ret.is_error())
						dwarnln("failed to create hardlink '{}': {}", file_name_sv, ret.error());
			}
			else if (file_type == SYMTYPE)
			{
				if (auto ret = parent_inode->create_file(file_name_sv, file_mode, file_uid, file_gid); ret.is_error())
					dwarnln("failed to create symlink '{}': {}", file_name_sv, ret.error());
				else
				{
					BAN::StringView link_name;

					char link_buffer[101] {};
					if (!next_link_name.empty())
						link_name = next_link_name.sv();
					else
					{
						const paddr_t paddr = module.start + offset;
						PageTable::with_fast_page(paddr & PAGE_ADDR_MASK, [&] {
							memcpy(link_buffer, PageTable::fast_page_as_ptr((paddr % PAGE_SIZE) + 157), 100);
						});
						link_name = link_buffer;
					}

					auto inode = TRY(parent_inode->find_inode(file_name_sv));
					TRY(inode->set_link_target(link_name));
				}
			}
			else
			{
				if (auto ret = parent_inode->create_file(file_name_sv, file_mode, file_uid, file_gid); ret.is_error())
					dwarnln("failed to create file '{}': {}", file_name_sv, ret.error());
				else
				{
					if (file_size)
					{
						auto inode = TRY(parent_inode->find_inode(file_name_sv));

						size_t nwritten = 0;
						while (nwritten < file_size)
						{
							const paddr_t paddr = module.start + offset + 512 + nwritten;
							PageTable::with_fast_page(paddr & PAGE_ADDR_MASK, [&] {
								memcpy(temp_page, PageTable::fast_page_as_ptr(), PAGE_SIZE);
							});

							const size_t page_off = paddr % PAGE_SIZE;
							const size_t to_write = BAN::Math::min(file_size - nwritten, PAGE_SIZE - page_off);
							TRY(inode->write(nwritten, { temp_page + page_off, to_write }));
							nwritten += to_write;
						}
					}
				}
			}

			if (file_type != 'L' && file_type != 'K')
			{
				next_file_name.clear();
				next_link_name.clear();
			}

			offset += 512 + file_size;
			if (auto rem = offset % 512)
				offset += 512 - rem;
		}

		return {};
	}

}
