#pragma once

#include <kernel/Storage/ATABus.h>
#include <kernel/Storage/StorageDevice.h>

namespace Kernel
{

	class ATADevice final : public StorageDevice
	{
	public:
		ATADevice(ATABus* bus) : m_bus(bus) { }
		BAN::ErrorOr<void> initialize(ATABus::DeviceType, const uint16_t*);

		virtual BAN::ErrorOr<void> read_sectors(uint64_t, uint8_t, uint8_t*) override;
		virtual BAN::ErrorOr<void> write_sectors(uint64_t, uint8_t, const uint8_t*) override;
		virtual uint32_t sector_size() const override { return m_sector_words * 2; }
		virtual uint64_t total_size() const override { return m_lba_count * sector_size(); }

		BAN::StringView model() const { return m_model; }

	private:
		ATABus* m_bus;
		uint8_t m_index;

		ATABus::DeviceType m_type;
		uint16_t m_signature;
		uint16_t m_capabilities;
		uint32_t m_command_set;
		uint32_t m_sector_words;
		uint64_t m_lba_count;
		char m_model[41];

		friend class ATABus;

	public:
		virtual ino_t ino() const override { return m_index; }
		virtual Mode mode() const override { return { Mode::IFBLK }; }
		virtual nlink_t nlink() const override { return 1; }
		virtual uid_t uid() const override { return 0; }
		virtual gid_t gid() const override { return 0; }
		virtual off_t size() const override { return 0; }
		virtual blksize_t blksize() const override { return sector_size(); }
		virtual blkcnt_t blocks() const override { return 0; }
		virtual dev_t dev() const override { return m_bus->controller()->dev(); }
		virtual dev_t rdev() const override { return 0x5429; }

		virtual BAN::StringView name() const override { return BAN::StringView(m_device_name, sizeof(m_device_name) - 1); }

		virtual BAN::ErrorOr<size_t> read(size_t, void*, size_t) override;

	public:
		char m_device_name[4] {};
	};

}