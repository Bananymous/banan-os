#pragma once

#include <BAN/Memory.h>
#include <kernel/FS/Inode.h>

namespace Kernel
{
	
	class FileSystem
	{
	public:
		virtual const BAN::RefCounted<Inode> root_inode() const = 0;
	};

}