#pragma once

#include <kernel/FS/Inode.h>
#include <kernel/Memory/MemoryRegion.h>

namespace Kernel::ELF
{

	struct LoadResult
	{
		struct TLS
		{
			vaddr_t addr;
			size_t size;
		};

		bool open_execfd;
		vaddr_t entry_point;
		BAN::Optional<TLS> master_tls;
		BAN::Vector<BAN::UniqPtr<MemoryRegion>> regions;
	};

	BAN::ErrorOr<LoadResult> load_from_inode(BAN::RefPtr<Inode> root, BAN::RefPtr<Inode> inode, const Credentials&, PageTable&);

}
