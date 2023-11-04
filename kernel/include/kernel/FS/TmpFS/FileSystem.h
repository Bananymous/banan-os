#pragma once

#include <BAN/HashMap.h>
#include <BAN/Iteration.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/FS/TmpFS/Inode.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	template<typename F>
	concept for_each_indirect_paddr_allocating_callback = requires(F func, paddr_t paddr, bool was_allocated)
	{
		requires BAN::is_same_v<decltype(func(paddr, was_allocated)), BAN::Iteration>;
	};

	class TmpFileSystem : public FileSystem
	{
	public:
		static constexpr size_t no_page_limit = SIZE_MAX;

	public:
		static BAN::ErrorOr<TmpFileSystem*> create(size_t max_pages, mode_t, uid_t, gid_t);
		~TmpFileSystem();

		virtual BAN::RefPtr<Inode> root_inode() override { return m_root_inode; }

		BAN::ErrorOr<BAN::RefPtr<TmpInode>> open_inode(ino_t ino);

		void read_inode(ino_t ino, TmpInodeInfo& out);
		void write_inode(ino_t ino, const TmpInodeInfo&);
		void delete_inode(ino_t ino);
		BAN::ErrorOr<ino_t> allocate_inode(const TmpInodeInfo&);

		void read_block(size_t index, BAN::ByteSpan buffer);
		void write_block(size_t index, BAN::ConstByteSpan buffer);
		void free_block(size_t index);
		BAN::ErrorOr<size_t> allocate_block();

	private:
		struct PageInfo
		{
			enum Flags : paddr_t
			{
				Present = 1 << 0,				
			};

			// 12 bottom bits of paddr can be used as flags, since
			// paddr will always be page aligned.
			static constexpr size_t  flag_bits = 12;
			static constexpr paddr_t flags_mask = (1 << flag_bits) - 1;
			static constexpr paddr_t paddr_mask = ~flags_mask;
			static_assert((1 << flag_bits) <= PAGE_SIZE);

			paddr_t paddr() const { return raw & paddr_mask; }
			paddr_t flags() const { return raw & flags_mask; }

			void set_paddr(paddr_t paddr) { raw = (raw & flags_mask) | (paddr & paddr_mask); }
			void set_flags(paddr_t flags) { raw = (raw & paddr_mask) | (flags & flags_mask); }

			paddr_t raw { 0 };
		};

		struct InodeLocation
		{
			paddr_t paddr;
			size_t index;
		};

	private:
		TmpFileSystem(size_t max_pages);
		BAN::ErrorOr<void> initialize(mode_t, uid_t, gid_t);

		InodeLocation find_inode(ino_t ino);

		paddr_t find_block(size_t index);

		template<for_each_indirect_paddr_allocating_callback F>
		BAN::ErrorOr<void> for_each_indirect_paddr_allocating(PageInfo page_info, F callback, size_t depth);
		template<for_each_indirect_paddr_allocating_callback F>
		BAN::ErrorOr<BAN::Iteration> for_each_indirect_paddr_allocating_internal(PageInfo page_info, F callback, size_t depth);

		paddr_t find_indirect(PageInfo root, size_t index, size_t depth);

	private:
		RecursiveSpinLock m_lock;

		BAN::HashMap<ino_t, BAN::RefPtr<TmpInode>> m_inode_cache;
		BAN::RefPtr<TmpDirectoryInode> m_root_inode;

		// We store pages with triple indirection.
		// With 64-bit pointers we can store 512^3 pages of data (512 GiB)
		// which should be enough for now.
		// In future this should be dynamically calculated based on maximum
		// number of pages for this file system.
		PageInfo m_data_pages {};
		static constexpr size_t first_data_page = 1;
		static constexpr size_t max_data_pages =
			(PAGE_SIZE / sizeof(PageInfo)) *
			(PAGE_SIZE / sizeof(PageInfo)) *
			(PAGE_SIZE / sizeof(PageInfo));

		// We store inodes in pages with double indirection.
		// With 64-bit pointers we can store 512^2 pages of inodes
		// which should be enough for now.
		// In future this should be dynamically calculated based on maximum
		// number of pages for this file system.
		PageInfo m_inode_pages;
		static constexpr size_t first_inode = 1;
		static constexpr size_t max_inodes =
			(PAGE_SIZE / sizeof(PageInfo)) *
			(PAGE_SIZE / sizeof(PageInfo)) *
			(PAGE_SIZE / sizeof(TmpInodeInfo));

		const size_t m_max_pages;
	};

}