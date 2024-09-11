#pragma once

#include <kernel/FS/Inode.h>
#include <kernel/Memory/MemoryRegion.h>

namespace Kernel
{

	struct SharedFileData : public BAN::RefCounted<SharedFileData>, public BAN::Weakable<SharedFileData>
	{
		~SharedFileData();

		void sync(size_t page_index);

		Mutex mutex;

		// FIXME: this should probably be ordered tree like map
		//        for fast lookup and less memory usage
		BAN::Vector<paddr_t> pages;
		BAN::RefPtr<Inode> inode;
		uint8_t page_buffer[PAGE_SIZE];
	};

	class FileBackedRegion final : public MemoryRegion
	{
		BAN_NON_COPYABLE(FileBackedRegion);
		BAN_NON_MOVABLE(FileBackedRegion);

	public:
		static BAN::ErrorOr<BAN::UniqPtr<FileBackedRegion>> create(BAN::RefPtr<Inode>, PageTable&, off_t offset, size_t size, AddressRange address_range, Type, PageTable::flags_t);
		~FileBackedRegion();

		virtual BAN::ErrorOr<void> msync(vaddr_t, size_t, int) override;

		virtual BAN::ErrorOr<BAN::UniqPtr<MemoryRegion>> clone(PageTable& new_page_table) override;

	protected:
		virtual BAN::ErrorOr<bool> allocate_page_containing_impl(vaddr_t vaddr, bool wants_write) override;

	private:
		FileBackedRegion(BAN::RefPtr<Inode>, PageTable&, off_t offset, ssize_t size, Type flags, PageTable::flags_t page_flags);

	private:
		BAN::RefPtr<Inode> m_inode;
		const off_t m_offset;

		BAN::Vector<paddr_t> m_dirty_pages;
		BAN::RefPtr<SharedFileData> m_shared_data;
	};

}
