#pragma once

#include <kernel/FS/FileSystem.h>
#include <kernel/Storage/StorageController.h>

namespace Kernel
{

	class VirtualFileSystem : public FileSystem
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static VirtualFileSystem& get();
		static bool is_initialized();

		virtual const BAN::RefCounted<Inode> root_inode() const override { return m_root_inode; }

		BAN::ErrorOr<BAN::RefCounted<Inode>> from_absolute_path(BAN::StringView);

	private:
		VirtualFileSystem() = default;
		BAN::ErrorOr<void> initialize_impl();

	private:
		BAN::RefCounted<Inode> m_root_inode;

		BAN::Vector<StorageController*> m_storage_controllers;
	};

}