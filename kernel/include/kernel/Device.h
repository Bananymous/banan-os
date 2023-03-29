#pragma once

#include <BAN/StringView.h>
#include <BAN/Vector.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/FS/Inode.h>
#include <kernel/SpinLock.h>

namespace Kernel
{

	class Device : public Inode
	{
	public:
		enum class DeviceType
		{
			BlockDevice,
			CharacterDevice,
			DeviceController
		};

		virtual ~Device() {}
		virtual DeviceType device_type() const = 0;
		virtual void update() {}
	};

	class BlockDevice : public Device
	{
	public:
		virtual DeviceType device_type() const override { return DeviceType::BlockDevice; }
	};

	class CharacterDevice : public Device
	{
	public:
		virtual DeviceType device_type() const override { return DeviceType::CharacterDevice; }
	};

	class DeviceManager final : public FileSystem, public Inode
	{
		BAN_NON_COPYABLE(DeviceManager);
		BAN_NON_MOVABLE(DeviceManager);

	public:
		static void initialize();
		static DeviceManager& get();

		void update();
		void add_device(Device*);

		virtual ino_t ino() const override { return 0; }
		virtual mode_t mode() const override { return IFDIR | IRUSR | IWUSR | IXUSR | IRGRP | IXGRP | IROTH | IXOTH; }
		virtual nlink_t nlink() const override { return 0; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual off_t size() const override { return 0; }
		virtual timespec atime() const override { return { 0, 0 }; }
		virtual timespec mtime() const override { return { 0, 0 }; }
		virtual timespec ctime() const override { return { 0, 0 }; }
		virtual blksize_t blksize() const override { return 0; }
		virtual blkcnt_t blocks() const override { return 0; }

		virtual BAN::StringView name() const override { return "device-manager"sv; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override { return BAN::Error::from_errno(EISDIR); }
		virtual BAN::ErrorOr<void> create_file(BAN::StringView, mode_t) override { return BAN::Error::from_errno(EINVAL); };

		virtual Type type() const override { return Type::DeviceManager; }
		virtual bool operator==(const Inode&) const override { return false; }

		virtual BAN::RefPtr<Inode> root_inode() override { return this; }

	protected:
		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> read_directory_inode_impl(BAN::StringView) override;
		virtual BAN::ErrorOr<BAN::Vector<BAN::String>> read_directory_entries_impl(size_t) override;

	private:
		DeviceManager() = default;

	private:
		SpinLock m_lock;
		BAN::Vector<Device*> m_devices;

		friend class BAN::RefPtr<DeviceManager>;
	};

}