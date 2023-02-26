#pragma once

#include <BAN/Errors.h>
#include <kernel/Storage/StorageController.h>

namespace Kernel
{

	struct ATABus;

	class ATADevice : public StorageDevice
	{
	public:
		virtual BAN::ErrorOr<void> read_sectors(uint64_t, uint8_t, uint8_t*) override;
		virtual uint32_t sector_size() const override { return sector_words * 2; }
		virtual uint64_t total_size() const override { return lba_count * sector_size(); }

	private:
		enum class Type
		{
			Unknown,
			ATA,
			ATAPI,
		};

		Type type;
		uint8_t slave_bit; // 0x00 for master, 0x10 for slave
		uint16_t signature;
		uint16_t capabilities;
		uint32_t command_set;
		uint32_t sector_words;
		uint64_t lba_count;
		char model[41];

		ATABus* bus;

		friend class ATAController;
	};

	struct ATABus
	{
		uint16_t base;
		uint16_t ctrl;
		ATADevice devices[2];

		uint8_t read(uint16_t);
		void read_buffer(uint16_t, uint16_t*, size_t);
		void write(uint16_t, uint8_t);
		BAN::ErrorOr<void> wait(bool);
		BAN::Error error();
	};

	class ATAController : public StorageController
	{
	public:
		static BAN::ErrorOr<ATAController*> create(const PCIDevice&);

	private:
		ATAController(const PCIDevice& device) : m_pci_device(device) {}
		BAN::ErrorOr<void> initialize();

	private:
		ATABus m_buses[2];
		const PCIDevice& m_pci_device;
	};

}