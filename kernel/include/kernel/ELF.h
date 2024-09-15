#pragma once

#include <kernel/FS/Inode.h>
#include <kernel/Memory/MemoryRegion.h>

namespace Kernel::ELF
{

	struct LoadResult
	{
		bool has_interpreter;
		vaddr_t entry_point;
		BAN::Vector<BAN::UniqPtr<MemoryRegion>> regions;
	};

	BAN::ErrorOr<LoadResult> load_from_inode(BAN::RefPtr<Inode>, const Credentials&, PageTable&);

}
