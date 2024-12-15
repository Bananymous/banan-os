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

		auto inode_location = find_inode(ino);
		PageTable::with_fast_page(inode_location.paddr, [&] {
			out = PageTable::fast_page_as_sized<TmpInodeInfo>(inode_location.index);
		});
	}

	void TmpFileSystem::write_inode(ino_t ino, const TmpInodeInfo& info)
	{
		LockGuard _(m_mutex);

		auto inode_location = find_inode(ino);
		PageTable::with_fast_page(inode_location.paddr, [&] {
			auto& inode_info = PageTable::fast_page_as_sized<TmpInodeInfo>(inode_location.index);
			inode_info = info;
		});
	}

	void TmpFileSystem::delete_inode(ino_t ino)
	{
		LockGuard _(m_mutex);

		auto inode_location = find_inode(ino);
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

		constexpr size_t inodes_per_page = PAGE_SIZE / sizeof(TmpInodeInfo);

		ino_t ino = first_inode;
		TRY(for_each_indirect_paddr_allocating(m_inode_pages, [&](paddr_t paddr, bool) {
			BAN::Iteration result = BAN::Iteration::Continue;
			PageTable::with_fast_page(paddr, [&] {
				for (size_t i = 0; i < inodes_per_page; i++, ino++)
				{
					auto& inode_info = PageTable::fast_page_as_sized<TmpInodeInfo>(i);
					if (inode_info.mode != 0)
						continue;
					inode_info = info;
					result = BAN::Iteration::Break;
					return;
				}
			});
			return result;
		}, 2));

		return ino;
	}

	TmpFileSystem::InodeLocation TmpFileSystem::find_inode(ino_t ino)
	{
		LockGuard _(m_mutex);

		ASSERT(ino >= first_inode);
		ASSERT(ino < max_inodes);

		constexpr size_t inodes_per_page = PAGE_SIZE / sizeof(TmpInodeInfo);

		size_t index_of_page = (ino - first_inode) / inodes_per_page;
		size_t index_in_page = (ino - first_inode) % inodes_per_page;

		return {
			.paddr = find_indirect(m_inode_pages, index_of_page, 2),
			.index = index_in_page
		};
	}

	void TmpFileSystem::free_block(size_t index)
	{
		LockGuard _(m_mutex);

		constexpr size_t addresses_per_page = PAGE_SIZE / sizeof(PageInfo);

		const size_t index_of_page = (index - first_data_page) / addresses_per_page;
		const size_t index_in_page = (index - first_data_page) % addresses_per_page;

		paddr_t page_containing = find_indirect(m_data_pages, index_of_page, 2);

		paddr_t paddr_to_free = 0;
		PageTable::with_fast_page(page_containing, [&] {
			auto& page_info = PageTable::fast_page_as_sized<PageInfo>(index_in_page);
			ASSERT(page_info.flags() & PageInfo::Flags::Present);
			paddr_to_free = page_info.paddr();
			m_used_pages--;

			page_info.set_paddr(0);
			page_info.set_flags(0);
		});
		Heap::get().release_page(paddr_to_free);
	}

	BAN::ErrorOr<size_t> TmpFileSystem::allocate_block()
	{
		LockGuard _(m_mutex);

		size_t result = first_data_page;
		TRY(for_each_indirect_paddr_allocating(m_data_pages, [&] (paddr_t, bool allocated) {
			if (allocated)
				return BAN::Iteration::Break;
			result++;
			return BAN::Iteration::Continue;
		}, 3));
		return result;
	}

	paddr_t TmpFileSystem::find_block(size_t index)
	{
		LockGuard _(m_mutex);

		ASSERT(index > 0);
		return find_indirect(m_data_pages, index - first_data_page, 3);
	}

	paddr_t TmpFileSystem::find_indirect(PageInfo root, size_t index, size_t depth)
	{
		LockGuard _(m_mutex);

		ASSERT(root.flags() & PageInfo::Flags::Present);
		if (depth == 0)
		{
			ASSERT(index == 0);
			return root.paddr();
		}

		constexpr size_t addresses_per_page = PAGE_SIZE / sizeof(PageInfo);

		size_t divisor = 1;
		for (size_t i = 1; i < depth; i++)
			divisor *= addresses_per_page;

		size_t index_of_page = index / divisor;
		size_t index_in_page = index % divisor;

		ASSERT(index_of_page < addresses_per_page);

		PageInfo next;
		PageTable::with_fast_page(root.paddr(), [&] {
			next = PageTable::fast_page_as_sized<PageInfo>(index_of_page);
		});

		return find_indirect(next, index_in_page, depth - 1);
	}

	template<TmpFuncs::for_each_indirect_paddr_allocating_callback F>
	BAN::ErrorOr<BAN::Iteration> TmpFileSystem::for_each_indirect_paddr_allocating_internal(PageInfo page_info, F callback, size_t depth)
	{
		LockGuard _(m_mutex);

		ASSERT(page_info.flags() & PageInfo::Flags::Present);
		if (depth == 0)
		{
			bool is_new_block = page_info.flags() & PageInfo::Flags::Internal;
			return callback(page_info.paddr(), is_new_block);
		}

		for (size_t i = 0; i < PAGE_SIZE / sizeof(PageInfo); i++)
		{
			PageInfo next_info;
			PageTable::with_fast_page(page_info.paddr(), [&] {
				next_info = PageTable::fast_page_as_sized<PageInfo>(i);
			});

			if (!(next_info.flags() & PageInfo::Flags::Present))
			{
				if (m_used_pages >= m_max_pages)
					return BAN::Error::from_errno(ENOSPC);
				paddr_t new_paddr = Heap::get().take_free_page();
				if (new_paddr == 0)
					return BAN::Error::from_errno(ENOMEM);
				m_used_pages++;

				PageTable::with_fast_page(new_paddr, [&] {
					memset(PageTable::fast_page_as_ptr(), 0x00, PAGE_SIZE);
				});

				next_info.set_paddr(new_paddr);
				next_info.set_flags(PageInfo::Flags::Present);

				PageTable::with_fast_page(page_info.paddr(), [&] {
					auto& to_update_info = PageTable::fast_page_as_sized<PageInfo>(i);
					to_update_info = next_info;
				});

				// Don't sync the internal bit to actual memory
				next_info.set_flags(PageInfo::Flags::Internal | PageInfo::Flags::Present);
			}

			auto result = TRY(for_each_indirect_paddr_allocating_internal(next_info, callback, depth - 1));
			switch (result)
			{
				case BAN::Iteration::Continue:
					break;
				case BAN::Iteration::Break:
					return BAN::Iteration::Break;
				default:
					ASSERT_NOT_REACHED();
			}
		}

		return BAN::Iteration::Continue;
	}

	template<TmpFuncs::for_each_indirect_paddr_allocating_callback F>
	BAN::ErrorOr<void> TmpFileSystem::for_each_indirect_paddr_allocating(PageInfo page_info, F callback, size_t depth)
	{
		LockGuard _(m_mutex);

		BAN::Iteration result = TRY(for_each_indirect_paddr_allocating_internal(page_info, callback, depth));
		ASSERT(result == BAN::Iteration::Break);
		return {};
	}

}
