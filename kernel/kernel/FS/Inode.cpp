#include <BAN/StringView.h>
#include <kernel/FS/Inode.h>
#include <kernel/FS/VirtualFileSystem.h>

namespace Kernel
{

	
	BAN::ErrorOr<BAN::Vector<BAN::RefPtr<Inode>>> Inode::directory_inodes()
	{
		for (const auto& mount : VirtualFileSystem::get().mount_points())
			if (*mount.inode == *this)
				return mount.target->root_inode()->directory_inodes_impl();
		return directory_inodes_impl();
	}

	BAN::ErrorOr<BAN::RefPtr<Inode>> Inode::directory_find(BAN::StringView name)
	{
		if (name == ".."sv)
			return directory_find_impl(name);
		for (const auto& mount : VirtualFileSystem::get().mount_points())
			if (*mount.inode == *this)
				return mount.target->root_inode()->directory_find_impl(name);
		return directory_find_impl(name);
	}

}