#pragma once

#include <kernel/DeviceManager.h>
#include <kernel/PCI.h>
#include <kernel/SpinLock.h>
#include <kernel/Storage/StorageController.h>

namespace Kernel
{

	struct ATABus;
	class ATAController;

	class ATADevice final : public StorageDevice
	{
	public:
		virtual BAN::ErrorOr<void> read_sectors(uint64_t, uint8_t, uint8_t*) override;
		virtual BAN::ErrorOr<void> write_sectors(uint64_t, uint8_t, const uint8_t*) override;
		virtual uint32_t sector_size() const override { return sector_words * 2; }
		virtual uint64_t total_size() const override { return lba_count * sector_size(); }

	private:
		enum class DeviceType
		{
			Unknown,
			ATA,
			ATAPI,
		};

		DeviceType type;
		uint8_t slave_bit; // 0x00 for master, 0x10 for slave
		uint16_t signature;
		uint16_t capabilities;
		uint32_t command_set;
		uint32_t sector_words;
		uint64_t lba_count;
		char model[41];

		ATABus* bus;
		ATAController* controller;

		friend class ATAController;

		char device_name[4] {};

	public:
		virtual ino_t ino() const override { return !!slave_bit; }
		virtual Mode mode() const override { return { Mode::IFBLK }; }
		virtual nlink_t nlink() const override { return 1; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual off_t size() const override { return 0; }
		virtual blksize_t blksize() const override { return sector_size(); }
		virtual blkcnt_t blocks() const override { return 0; }
		virtual dev_t dev() const override;
		virtual dev_t rdev() const override { return 0x5429; }

		virtual BAN::StringView name() const override { return BAN::StringView(device_name, sizeof(device_name) - 1); }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;
	};

	struct ATABus
	{
		uint16_t base;
		uint16_t ctrl;
		ATADevice devices[2];

		uint8_t read(uint16_t);
		void read_buffer(uint16_t, uint16_t*, size_t);
		void write(uint16_t, uint8_t);
		void write_buffer(uint16_t, const uint16_t*, size_t);
		BAN::ErrorOr<void> wait(bool);
		BAN::Error error();
	};

	class ATAController final : public StorageController
	{
	public:
		static BAN::ErrorOr<ATAController*> create(const PCIDevice&);

	private:
		ATAController(const PCIDevice& device) : m_pci_device(device) {}
		BAN::ErrorOr<void> initialize();

		BAN::ErrorOr<void> read(ATADevice*, uint64_t, uint8_t, uint8_t*);
		BAN::ErrorOr<void> write(ATADevice*, uint64_t, uint8_t, const uint8_t*);

	private:
		ATABus m_buses[2];
		const PCIDevice& m_pci_device;
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