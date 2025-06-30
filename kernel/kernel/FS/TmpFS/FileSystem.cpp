#include <BAN/ScopeGuard.h>
#include <kernel/Device/DeviceNumbers.h>
#include <kernel/FS/TmpFS/FileSystem.h>
#include <kernel/Memory/Heap.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	static dev_t get_next_rdev()
	{
		static dev_t minor = 0;
		return makedev(DeviceNumber::TmpFS, minor++);
	}

	BAN::ErrorOr<TmpFileSystem*> TmpFileSystem::create(size_t max_pages, mode_t mode, uid_t uid, gid_t gid)
	{
		if (max_pages < 2)
			return BAN::Error::from_errno(ENOSPC);

		auto* result = new TmpFileSystem(max_pages);
		if (result == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		TRY(result->initialize(mode, uid, gid));
		return result;
	}

	TmpFileSystem::TmpFileSystem(size_t max_pages)
		: m_rdev(get_next_rdev())
		, m_max_pages(max_pages)
	{ }

	BAN::ErrorOr<void> TmpFileSystem::initialize(mode_t mode, uid_t uid, gid_t gid)
	{
		paddr_t data_paddr = Heap::get().take_free_page();
		if (data_paddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		m_used_pages++;

		m_data_pages.set_paddr(data_paddr);
		m_data_pages.set_flags(PageInfo::Flags::Present);
		PageTable::with_fast_page(data_paddr, [&] {
			memset(PageTable::fast_page_as_ptr(), 0x00, PAGE_SIZE);
		});

		paddr_t inodes_paddr = Heap::get().take_free_page();
		if (inodes_paddr == 0)
			return BAN::Error::from_errno(ENOMEM);
		m_used_pages++;

		m_inode_pages.set_paddr(inodes_paddr);
		m_inode_pages.set_flags(PageInfo::Flags::Present);
		PageTable::with_fast_page(inodes_paddr, [&] {
			memset(PageTable::fast_page_as_ptr(), 0x00, PAGE_SIZE);
		});

		m_root_inode = TRY(TmpDirectoryInode::create_root(*this, mode, uid, gid));

		return {};
	}

	TmpFileSystem::~TmpFileSystem()
	{
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<BAN::RefPtr<TmpInode>> TmpFileSystem::open_inode(ino_t ino)
	{
		LockGuard _(m_mutex);

		auto it = m_inode_cache.find(ino);
		if (it != m_inode_cache.end())
			return it->value;

		TmpInodeInfo inode_info;

		auto inode_location = find_inode(ino);
		PageTable::with_fast_page(inode_location.paddr, [&] {
			inode_info = PageTable::fast_page_as_sized<TmpInodeInfo>(inode_location.index);
		});

		auto inode = TRY(TmpInode::create_from_existing(*this, ino, inode_info));
		TRY(m_inode_cache.insert(ino, inode));
		return inode;
	}

	BAN::ErrorOr<void> TmpFileSystem::add_to_cache(BAN::RefPtr<TmpInode> inode)
	{
		LockGuard _(m_mutex);

		if (!m_inode_cache.contains(inode->ino()))
			TRY(m_inode_cache.insert(inode->ino(), inode));
		return {};
	}

	void TmpFileSystem::remove_from_cache(BAN::RefPtr<TmpInode> inode)
	{
		LockGuard _(m_mutex);

		ASSERT(m_inode_cache.contains(inode->ino()));
		m_inode_cache.remove(inode->ino());
	}

	void TmpFileSystem::read_inode(ino_t ino, TmpInodeInfo& out)
	{
		LockGuard _(m_mutex);

		const auto inode_location = find_inode(ino);
		PageTable::with_fast_page(inode_location.paddr, [&] {
			out = PageTable::fast_page_as_sized<TmpInodeInfo>(inode_location.index);
		});
	}

	void TmpFileSystem::write_inode(ino_t ino, const TmpInodeInfo& info)
	{
		LockGuard _(m_mutex);

		const auto inode_location = find_inode(ino);
		PageTable::with_fast_page(inode_location.paddr, [&] {
			auto& inode_info = PageTable::fast_page_as_sized<TmpInodeInfo>(inode_location.index);
			inode_info = info;
		});
	}

	void TmpFileSystem::delete_inode(ino_t ino)
	{
		LockGuard _(m_mutex);

		const auto inode_location = find_inode(ino);
		PageTable::with_fast_page(inode_location.paddr, [&] {
			auto& inode_info = PageTable::fast_page_as_sized<TmpInodeInfo>(inode_location.index);
			ASSERT(inode_info.nlink == 0);
			for (auto paddr : inode_info.block)
				ASSERT(paddr == 0);
			inode_info = {};
		});

		ASSERT(!m_inode_cache.contains(ino));
	}

	BAN::ErrorOr<ino_t> TmpFileSystem::allocate_inode(const TmpInodeInfo& info)
	{
		LockGuard _(m_mutex);

		constexpr size_t inode_infos_per_page = PAGE_SIZE / sizeof(TmpInodeInfo);
		constexpr size_t page_infos_per_page = PAGE_SIZE / sizeof(PageInfo);

		for (size_t layer0_index = 0; layer0_index < page_infos_per_page; layer0_index++)
		{
			PageInfo layer0_page;
			PageTable::with_fast_page(m_inode_pages.paddr(), [&] {
				layer0_page = PageTable::fast_page_as_sized<PageInfo>(layer0_index);
			});

			if (!(layer0_page.flags() & PageInfo::Flags::Present))
			{
				if (m_used_pages >= m_max_pages)
					return BAN::Error::from_errno(ENOSPC);
				const paddr_t paddr = Heap::get().take_free_page();
				if (paddr == 0)
					return BAN::Error::from_errno(ENOMEM);
				PageTable::with_fast_page(paddr, [&] {
					memset(PageTable::fast_page_as_ptr(), 0, PAGE_SIZE);
				});
				PageTable::with_fast_page(m_inode_pages.paddr(), [&] {
					auto& page_info = PageTable::fast_page_as_sized<PageInfo>(layer0_index);
					page_info.set_paddr(paddr);
					page_info.set_flags(PageInfo::Flags::Present);
					layer0_page = page_info;
				});
				m_used_pages++;
			}

			for (size_t layer1_index = 0; layer1_index < page_infos_per_page; layer1_index++)
			{
				PageInfo layer1_page;
				PageTable::with_fast_page(layer0_page.paddr(), [&] {
					layer1_page = PageTable::fast_page_as_sized<PageInfo>(layer1_index);
				});

				if (!(layer1_page.flags() & PageInfo::Flags::Present))
				{
					if (m_used_pages >= m_max_pages)
						return BAN::Error::from_errno(ENOSPC);
					const paddr_t paddr = Heap::get().take_free_page();
					if (paddr == 0)
						return BAN::Error::from_errno(ENOMEM);
					PageTable::with_fast_page(paddr, [&] {
						memset(PageTable::fast_page_as_ptr(), 0, PAGE_SIZE);
					});
					PageTable::with_fast_page(layer0_page.paddr(), [&] {
						auto& page_info = PageTable::fast_page_as_sized<PageInfo>(layer1_index);
						page_info.set_paddr(paddr);
						page_info.set_flags(PageInfo::Flags::Present);
						layer1_page = page_info;
					});
					m_used_pages++;
				}

				size_t layer2_index = SIZE_MAX;

				PageTable::with_fast_page(layer1_page.paddr(), [&] {
					for (size_t i = 0; i < PAGE_SIZE / sizeof(TmpInodeInfo); i++)
					{
						auto& inode_info = PageTable::fast_page_as_sized<TmpInodeInfo>(i);
						if (inode_info.mode != 0)
							continue;
						inode_info = info;
						layer2_index = i;
						return;
					}
				});

				if (layer2_index != SIZE_MAX)
				{
					const size_t layer0_offset = layer0_index * inode_infos_per_page * page_infos_per_page;
					const size_t layer1_offset = layer1_index * inode_infos_per_page;
					const size_t layer2_offset = layer2_index;
					return layer0_offset + layer1_offset + layer2_offset + first_inode;
				}
			}
		}

		ASSERT_NOT_REACHED();
	}

	TmpFileSystem::InodeLocation TmpFileSystem::find_inode(ino_t ino)
	{
		LockGuard _(m_mutex);

		ASSERT(ino >= first_inode);
		ASSERT(ino - first_inode < max_inodes);

		constexpr size_t inode_infos_per_page = PAGE_SIZE / sizeof(TmpInodeInfo);
		constexpr size_t page_infos_per_page = PAGE_SIZE / sizeof(PageInfo);
		const size_t layer0_index = (ino - first_inode) / inode_infos_per_page / page_infos_per_page;
		const size_t layer1_index = (ino - first_inode) / inode_infos_per_page % page_infos_per_page;
		const size_t layer2_index = (ino - first_inode) % inode_infos_per_page;
		ASSERT(layer0_index < page_infos_per_page);

		PageInfo layer0_page;
		PageTable::with_fast_page(m_inode_pages.paddr(), [&] {
			layer0_page = PageTable::fast_page_as_sized<PageInfo>(layer0_index);
		});
		ASSERT(layer0_page.flags() & PageInfo::Flags::Present);

		PageInfo layer1_page;
		PageTable::with_fast_page(layer0_page.paddr(), [&] {
			layer1_page = PageTable::fast_page_as_sized<PageInfo>(layer1_index);
		});
		ASSERT(layer1_page.flags() & PageInfo::Flags::Present);

		return {
			.paddr = layer1_page.paddr(),
			.index = layer2_index,
		};
	}

	void TmpFileSystem::free_block(size_t index)
	{
		LockGuard _(m_mutex);

		ASSERT(index >= first_data_page);
		ASSERT(index - first_data_page < max_data_pages);

		constexpr size_t page_infos_per_page = PAGE_SIZE / sizeof(PageInfo);
		const size_t layer0_index = (index - first_data_page) / (page_infos_per_page - 1) / page_infos_per_page;
		const size_t layer1_index = (index - first_data_page) / (page_infos_per_page - 1) % page_infos_per_page;
		const size_t layer2_index = (index - first_data_page) % (page_infos_per_page - 1);
		ASSERT(layer0_index < page_infos_per_page);

		PageInfo layer0_page;
		PageTable::with_fast_page(m_data_pages.paddr(), [&] {
			layer0_page = PageTable::fast_page_as_sized<PageInfo>(layer0_index);
		});
		ASSERT(layer0_page.flags() & PageInfo::Flags::Present);

		PageInfo layer1_page;
		PageTable::with_fast_page(layer0_page.paddr(), [&] {
			layer1_page = PageTable::fast_page_as_sized<PageInfo>(layer1_index);
		});
		ASSERT(layer1_page.flags() & PageInfo::Flags::Present);

		paddr_t page_to_free;
		PageTable::with_fast_page(layer1_page.paddr(), [&] {
			auto& allocated_pages = PageTable::fast_page_as_sized<size_t>(page_infos_per_page - 1);
			ASSERT(allocated_pages > 0);
			allocated_pages--;

			auto& page_info = PageTable::fast_page_as_sized<PageInfo>(layer2_index);
			ASSERT(page_info.flags() & PageInfo::Flags::Present);
			page_to_free = page_info.paddr();
			page_info.set_paddr(0);
			page_info.set_flags(0);
		});

		Heap::get().release_page(page_to_free);
	}

	paddr_t TmpFileSystem::find_block(size_t index)
	{
		LockGuard _(m_mutex);

		ASSERT(index >= first_data_page);
		ASSERT(index - first_data_page < max_data_pages);

		constexpr size_t page_infos_per_page = PAGE_SIZE / sizeof(PageInfo);
		const size_t layer0_index = (index - first_data_page) / (page_infos_per_page - 1) / page_infos_per_page;
		const size_t layer1_index = (index - first_data_page) / (page_infos_per_page - 1) % page_infos_per_page;
		const size_t layer2_index = (index - first_data_page) % (page_infos_per_page - 1);
		ASSERT(layer0_index < page_infos_per_page);

		PageInfo layer0_page;
		PageTable::with_fast_page(m_data_pages.paddr(), [&] {
			layer0_page = PageTable::fast_page_as_sized<PageInfo>(layer0_index);
		});
		ASSERT(layer0_page.flags() & PageInfo::Flags::Present);

		PageInfo layer1_page;
		PageTable::with_fast_page(layer0_page.paddr(), [&] {
			layer1_page = PageTable::fast_page_as_sized<PageInfo>(layer1_index);
		});
		ASSERT(layer1_page.flags() & PageInfo::Flags::Present);

		PageInfo layer2_page;
		PageTable::with_fast_page(layer1_page.paddr(), [&] {
			layer2_page = PageTable::fast_page_as_sized<PageInfo>(layer2_index);
		});
		ASSERT(layer2_page.flags() & PageInfo::Flags::Present);

		return layer2_page.paddr();
	}

	BAN::ErrorOr<size_t> TmpFileSystem::allocate_block()
	{
		LockGuard _(m_mutex);

		if (m_used_pages >= m_max_pages)
			return BAN::Error::from_errno(ENOSPC);

		const paddr_t new_block = Heap::get().take_free_page();
		if (new_block == 0)
			return BAN::Error::from_errno(ENOMEM);
		PageTable::with_fast_page(new_block, [] {
			memset(PageTable::fast_page_as_ptr(), 0, PAGE_SIZE);
		});
		BAN::ScopeGuard block_deleter([new_block] { Heap::get().release_page(new_block); });

		constexpr size_t page_infos_per_page = PAGE_SIZE / sizeof(PageInfo);

		for (size_t layer0_index = 0; layer0_index < PAGE_SIZE / sizeof(PageInfo); layer0_index++)
		{
			PageInfo layer0_page;
			PageTable::with_fast_page(m_data_pages.paddr(), [&] {
				layer0_page = PageTable::fast_page_as_sized<PageInfo>(layer0_index);
			});

			if (!(layer0_page.flags() & PageInfo::Flags::Present))
			{
				if (m_used_pages + 1 >= m_max_pages)
					return BAN::Error::from_errno(ENOSPC);
				const paddr_t paddr = Heap::get().take_free_page();
				if (paddr == 0)
					return BAN::Error::from_errno(ENOMEM);
				PageTable::with_fast_page(paddr, [&] {
					memset(PageTable::fast_page_as_ptr(), 0, PAGE_SIZE);
				});
				PageTable::with_fast_page(m_data_pages.paddr(), [&] {
					auto& page_info = PageTable::fast_page_as_sized<PageInfo>(layer0_index);
					page_info.set_paddr(paddr);
					page_info.set_flags(PageInfo::Flags::Present);
					layer0_page = page_info;
				});
				m_used_pages++;
			}

			for (size_t layer1_index = 0; layer1_index < PAGE_SIZE / sizeof(PageInfo); layer1_index++)
			{
				PageInfo layer1_page;
				PageTable::with_fast_page(layer0_page.paddr(), [&] {
					layer1_page = PageTable::fast_page_as_sized<PageInfo>(layer1_index);
				});

				if (!(layer1_page.flags() & PageInfo::Flags::Present))
				{
					if (m_used_pages + 1 >= m_max_pages)
						return BAN::Error::from_errno(ENOSPC);
					const paddr_t paddr = Heap::get().take_free_page();
					if (paddr == 0)
						return BAN::Error::from_errno(ENOMEM);
					PageTable::with_fast_page(paddr, [&] {
						memset(PageTable::fast_page_as_ptr(), 0, PAGE_SIZE);
					});
					PageTable::with_fast_page(layer0_page.paddr(), [&] {
						auto& page_info = PageTable::fast_page_as_sized<PageInfo>(layer1_index);
						page_info.set_paddr(paddr);
						page_info.set_flags(PageInfo::Flags::Present);
						layer1_page = page_info;
					});
					m_used_pages++;
				}

				size_t layer2_index = SIZE_MAX;

				PageTable::with_fast_page(layer1_page.paddr(), [&] {
					constexpr size_t pages_per_block = page_infos_per_page - 1;

					auto& allocated_pages = PageTable::fast_page_as_sized<size_t>(pages_per_block);
					if (allocated_pages == pages_per_block)
						return;

					for (size_t i = 0; i < pages_per_block; i++)
					{
						auto& page_info = PageTable::fast_page_as_sized<PageInfo>(i);
						if (page_info.flags() & PageInfo::Flags::Present)
							continue;
						page_info.set_paddr(new_block);
						page_info.set_flags(PageInfo::Flags::Present);
						allocated_pages++;
						layer2_index = i;
						return;
					}

					ASSERT_NOT_REACHED();
				});

				if (layer2_index != SIZE_MAX)
				{
					block_deleter.disable();

					m_used_pages++;

					const size_t layer0_offset = layer0_index * (page_infos_per_page - 1) * page_infos_per_page;
					const size_t layer1_offset = layer1_index * (page_infos_per_page - 1);
					const size_t layer2_offset = layer2_index;
					return layer0_offset + layer1_offset + layer2_offset + first_data_page;
				}
			}
		}

		ASSERT_NOT_REACHED();
	}

}
