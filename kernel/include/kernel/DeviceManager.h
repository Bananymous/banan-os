#pragma once

#include <BAN/StringView.h>
#include <kernel/FS/FileSystem.h>
#include <kernel/SpinLock.h>

#include <sys/sysmacros.h>

namespace Kernel
{

	class Device;

	class DeviceManager final : public FileSystem, public Inode
	{
		BAN_NON_COPYABLE(DeviceManager);
		BAN_NON_MOVABLE(DeviceManager);

	public:
		static DeviceManager& get();

		void initialize_pci_devices();
		void initialize_updater();

		ino_t get_next_ino() const;
		dev_t get_next_rdev() const;
		uint8_t get_next_input_dev() const;

		void update();
		void add_device(Device*);

		virtual BAN::RefPtr<Inode> root_inode() override { return this; }

		virtual BAN::StringView name() const override { return "device-manager"; }

		virtual BAN::ErrorOr<BAN::RefPtr<Inode>> directory_find_inode(BAN::StringView) override;
		virtual BAN::ErrorOr<void> directory_read_next_entries(off_t, DirectoryEntryList*, size_t) override;

	private:
		DeviceManager() = default;

	private:
		SpinLock m_lock;
		BAN::Vector<Device*> m_devices;

		friend class BAN::RefPtr<DeviceManager>;

	public:
		virtual ino_t ino() const override { return 0; }
		virtual Mode mode() const override { return { Mode::IFDIR | Mode::IRUSR | Mode::IWUSR | Mode::IXUSR | Mode::IRGRP | Mode::IXGRP | Mode::IROTH | Mode::IXOTH }; }
		virtual nlink_t nlink() const override { return 1; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual off_t size() const override { return 0; }
		virtual timespec atime() const override { return { 0, 0 }; }
		virtual timespec mtime() const override { return { 0, 0 }; }
		virtual timespec ctime() const override { return { 0, 0 }; }
		virtual blksize_t blksize() const override { ASSERT(m_blksize); return m_blksize; }
		virtual blkcnt_t blocks() const override { return 0; }
		virtual dev_t dev() const override { return makedev(0, 5); }
		virtual dev_t rdev() const override { return 0; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t)        { return BAN::Error::from_errno(EISDIR); }
		virtual BAN::ErrorOr<void> create_file(BAN::StringView, mode_t) { return BAN::Error::from_errno(ENOTSUP); }

		void set_blksize(blksize_t blksize) { m_blksize = blksize; }

	private:
		blksize_t m_blksize = 0;
	};

}