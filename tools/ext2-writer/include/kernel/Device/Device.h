#pragma once

#include <kernel/FS/Inode.h>

namespace Kernel
{

	class BlockDevice : public BAN::RefCounted<BlockDevice>
	{
	public:
		virtual BAN::ErrorOr<void> read_blocks(uint64_t first_block, size_t block_count, BAN::ByteSpan) = 0;
		virtual BAN::ErrorOr<void> write_blocks(uint64_t first_block, size_t block_count, BAN::ConstByteSpan) = 0;
		virtual BAN::ErrorOr<void> sync_blocks(uint64_t block, size_t block_count) = 0;
		virtual blksize_t blksize() const = 0;
	};

}
