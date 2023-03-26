#include <BAN/StringView.h>
#include <kernel/FS/Inode.h>
#include <kernel/FS/VirtualFileSystem.h>

namespace Kernel
{

	BAN::ErrorOr<BAN::RefPtr<Inode>> Inode::read_directory_inode(BAN::StringView name)
	{
		if (name == ".."sv)
			return read_directory_inode_impl(name);
		for (const auto& mount : VirtualFileSystem::get().mount_points())
			if (*mount.inode == *this)
				return mount.target->root_inode()->read_directory_inode_impl(name);
		return read_directory_inode_impl(name);
	}

	BAN::ErrorOr<BAN::Vector<BAN::String>> Inode::read_directory_entries(size_t index)
	{
		for (const auto& mount : VirtualFileSystem::get().mount_points())
			if (*mount.inode == *this)
				return mount.target->root_inode()->read_directory_entries_impl(index);
		return read_directory_entries_impl(index);
	}

}