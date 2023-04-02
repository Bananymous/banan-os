#pragma once

#include <kernel/DeviceManager.h>
#include <kernel/PCI.h>
#include <kernel/SpinLock.h>
#include <kernel/Storage/StorageController.h>

namespace Kernel
{

	class ATADevice;

	class ATAController final : public StorageController
	{
	public:
		static BAN::ErrorOr<ATAController*> create(const PCIDevice&);

	private:
		ATAController() = default;
		BAN::ErrorOr<void> initialize(const PCIDevice& device);

		BAN::ErrorOr<void> read(ATADevice*, uint64_t, uint8_t, uint8_t*);
		BAN::ErrorOr<void> write(ATADevice*, uint64_t, uint8_t, const uint8_t*);

	private:
		SpinLock m_lock;

		friend class ATADevice;

	public:
		virtual ino_t ino() const override { return 0; }
		virtual Mode mode() const override { return { Mode::IFCHR }; }
		virtual nlink_t nlink() const override { return 1; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual off_t size() const override { return 0; }
		virtual blksize_t blksize() const override { return 0; }
		virtual blkcnt_t blocks() const override { return 0; }
		virtual dev_t dev() const override { return DeviceManager::get().dev(); }
		virtual dev_t rdev() const override { return 0x8594; }

		virtual BAN::StringView name() const override { return "hd"sv; }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) { return BAN::Error::from_errno(ENOTSUP); }
	};

}