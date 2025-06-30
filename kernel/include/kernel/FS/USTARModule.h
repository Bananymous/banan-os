#pragma once

#include <kernel/BootInfo.h>
#include <kernel/FS/FileSystem.h>

namespace Kernel
{

	bool is_ustar_boot_module(const BootModule&);
	BAN::ErrorOr<void> unpack_boot_module_into_filesystem(BAN::RefPtr<FileSystem>, const BootModule&);

}
