#pragma once

#include <BAN/Memory.h>
#include <kernel/FS/Inode.h>

namespace Kernel
{
	
	class FileSystem
	{
	public:
		virtual const BAN::RefPtr<Inode> root_inode() const = 0;
	};

}