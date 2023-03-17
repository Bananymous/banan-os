#pragma once

#include <BAN/HashMap.h>
#include <BAN/String.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/Storage/StorageController.h>

namespace Kernel
{

	class VirtualFileSystem : public FileSystem
	{
	public:
		static BAN::ErrorOr<void> initialize();
		static VirtualFileSystem& get();
		virtual ~VirtualFileSystem() {};

		virtual const BAN::RefPtr<Inode> root_inode() const override  { return m_root_inode; }

		struct File
		{
			BAN::RefPtr<Inode> inode;
			BAN::String canonical_path;
		};
		BAN::ErrorOr<File> file_from_absolute_path(BAN::StringView);

	private:
		VirtualFileSystem() = default;
		BAN::ErrorOr<void> initialize_impl();

	private:
		BAN::RefPtr<Inode>				m_root_inode;
		BAN::Vector<StorageController*>	m_storage_controllers;
	};

}