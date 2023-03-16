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

		virtual const BAN::RefPtr<Inode> root_inode() const override;

		void close_inode(BAN::StringView);

		BAN::ErrorOr<BAN::RefPtr<Inode>> from_absolute_path(BAN::StringView);

	private:
		VirtualFileSystem() = default;
		BAN::ErrorOr<void> initialize_impl();

	private:
		BAN::HashMap<BAN::String, BAN::RefPtr<Inode>>	m_open_inodes;
		BAN::Vector<StorageController*>					m_storage_controllers;
	};

}