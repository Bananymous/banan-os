#include <kernel/FS/DevFS/FileSystem.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Storage/ATA/ATABus.h>
#include <kernel/Storage/ATA/ATADefinitions.h>
#include <kernel/Storage/ATA/ATADevice.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	static constexpr uint64_t s_ata_timeout_ms = 100;

	BAN::ErrorOr<BAN::RefPtr<ATABus>> ATABus::create(uint16_t base, uint16_t ctrl, uint8_t irq)
	{
		auto* bus_ptr = new ATABus(base, ctrl);
		if (bus_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto bus = BAN::RefPtr<ATABus>::adopt(bus_ptr);
		bus->set_irq(irq);
		TRY(bus->initialize());
		return bus;
	}

	BAN::ErrorOr<void> ATABus::initialize()
	{
		enable_interrupt();

		BAN::Vector<uint16_t> identify_buffer;
		MUST(identify_buffer.resize(256));

		for (uint8_t i = 0; i < 2; i++)
		{
			bool is_secondary = (i == 1);

			DeviceType device_type;
			if (auto res = identify(is_secondary, identify_buffer.span()); res.is_error())
				continue;
			else
				device_type = res.value();

			// Enable interrupts
			select_device(is_secondary);
			io_write(ATA_PORT_CONTROL, 0);

			auto device_or_error = ATADevice::create(this, device_type, is_secondary, identify_buffer.span());

			if (device_or_error.is_error())
			{
				dprintln("{}", device_or_error.error());
				continue;
			}

			auto device = device_or_error.release_value();
			TRY(m_devices.push_back(device.ptr()));
		}

		return {};
	}

	static void select_delay()
	{
		auto time = SystemTimer::get().ns_since_boot();
		while (SystemTimer::get().ns_since_boot() < time + 400)
			continue;
	}

	void ATABus::select_device(bool secondary)
	{
		io_write(ATA_PORT_DRIVE_SELECT, 0xA0 | ((uint8_t)secondary << 4));
		select_delay();
	}

	static bool identify_all_same(BAN::Span<const uint16_t> identify_data)
	{
		uint16_t value = identify_data[0];
		for (size_t i = 1; i < 256; i++)
			if (identify_data[i] != value)
				return false;
		return true;
	}

	BAN::ErrorOr<ATABus::DeviceType> ATABus::identify(bool secondary, BAN::Span<uint16_t> buffer)
	{
		// Try to detect whether port contains device
		uint8_t status = io_read(ATA_PORT_STATUS);
		if (status & ATA_STATUS_BSY)
		{
			uint64_t timeout = SystemTimer::get().ms_since_boot() + s_ata_timeout_ms;
			while ((status = io_read(ATA_PORT_STATUS)) & ATA_STATUS_BSY)
			{
				if (SystemTimer::get().ms_since_boot() >= timeout)
				{
					dprintln("BSY flag clear timeout, assuming no drive on port");
					return BAN::Error::from_errno(ETIMEDOUT);
				}
			}
		}
		if (__builtin_popcount(status) >= 4)
		{
			dprintln("STATUS contains garbage, assuming no drive on port");
			return BAN::Error::from_errno(EINVAL);
		}

		select_device(secondary);

		// Disable interrupts
		io_write(ATA_PORT_CONTROL, ATA_CONTROL_nIEN);

		io_write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY);
		SystemTimer::get().sleep(1);

		// No device on port
		if (io_read(ATA_PORT_STATUS) == 0)
			return BAN::Error::from_errno(EINVAL);

		DeviceType type = DeviceType::ATA;

		if (wait(true).is_error())
		{
			uint8_t lba1 = io_read(ATA_PORT_LBA1);
			uint8_t lba2 = io_read(ATA_PORT_LBA2);

			if (lba1 == 0x14 && lba2 == 0xEB)
				type = DeviceType::ATAPI;
			else if (lba1 == 0x69 && lba2 == 0x96)
				type = DeviceType::ATAPI;
			else
			{
				dprintln("Unsupported device type");
				return BAN::Error::from_errno(EINVAL);
			}

			io_write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY_PACKET);
			SystemTimer::get().sleep(1);

			if (auto res = wait(true); res.is_error())
			{
				dprintln("Fatal error: {}", res.error());
				return BAN::Error::from_errno(EINVAL);
			}
		}

		ASSERT(buffer.size() >= 256);
		read_buffer(ATA_PORT_DATA, buffer.data(), 256);

		if (identify_all_same(buffer))
			return BAN::Error::from_errno(ENODEV);

		return type;
	}

	void ATABus::handle_irq()
	{
		ASSERT(!m_has_got_irq);
		if (io_read(ATA_PORT_STATUS) & ATA_STATUS_ERR)
			dprintln("ATA Error: {}", error());
		m_has_got_irq = true;
	}

	void ATABus::block_until_irq()
	{
		while (!__sync_bool_compare_and_swap(&m_has_got_irq, true, false))
			__builtin_ia32_pause();
	}

	uint8_t ATABus::io_read(uint16_t port)
	{
		if (port <= 0x07)
			return IO::inb(m_base + port);
		if (0x10 <= port && port <= 0x11)
			return IO::inb(m_ctrl + port - 0x10);
		ASSERT_NOT_REACHED();
	}

	void ATABus::read_buffer(uint16_t port, uint16_t* buffer, size_t words)
	{
		if (port <= 0x07)
			return IO::insw(m_base + port - 0x00, buffer, words);
		if (0x10 <= port && port <= 0x11)
			return IO::insw(m_ctrl + port - 0x10, buffer, words);
		ASSERT_NOT_REACHED();
	}

	void ATABus::io_write(uint16_t port, uint8_t data)
	{
		if (port <= 0x07)
			return IO::outb(m_base + port, data);
		if (0x10 <= port && port <= 0x11)
			return IO::outb(m_ctrl + port - 0x10, data);
		ASSERT_NOT_REACHED();
	}

	void ATABus::write_buffer(uint16_t port, const uint16_t* buffer, size_t words)
	{
		uint16_t io_port = 0;
		if (port <= 0x07)
			io_port = m_base + port;
		if (0x10 <= port && port <= 0x11)
			io_port = m_ctrl + port - 0x10;
		ASSERT(io_port);
		for (size_t i = 0; i < words; i++)
			IO::outw(io_port, buffer[i]);
	}

	BAN::ErrorOr<void> ATABus::wait(bool wait_drq)
	{
		for (uint32_t i = 0; i < 4; i++)
			io_read(ATA_PORT_ALT_STATUS);

		uint64_t timeout = SystemTimer::get().ms_since_boot() + s_ata_timeout_ms;

		uint8_t status;
		while ((status = io_read(ATA_PORT_STATUS)) & ATA_STATUS_BSY)
			if (SystemTimer::get().ms_since_boot() >= timeout)
				return BAN::Error::from_errno(ETIMEDOUT);

		while (wait_drq && !(status & ATA_STATUS_DRQ))
		{
			if (SystemTimer::get().ms_since_boot() >= timeout)
				return BAN::Error::from_errno(ETIMEDOUT);
			if (status & ATA_STATUS_ERR)
				return error();
			if (status & ATA_STATUS_DF)
				return BAN::Error::from_errno(EIO);
			status = io_read(ATA_PORT_STATUS);
		}

		return {};
	}

	BAN::Error ATABus::error()
	{
		uint8_t err = io_read(ATA_PORT_ERROR);
		if (err & ATA_ERROR_AMNF)
			return BAN::Error::from_error_code(ErrorCode::ATA_AMNF);
		if (err & ATA_ERROR_TKZNF)
			return BAN::Error::from_error_code(ErrorCode::ATA_TKZNF);
		if (err & ATA_ERROR_ABRT)
			return BAN::Error::from_error_code(ErrorCode::ATA_ABRT);
		if (err & ATA_ERROR_MCR)
			return BAN::Error::from_error_code(ErrorCode::ATA_MCR);
		if (err & ATA_ERROR_IDNF)
			return BAN::Error::from_error_code(ErrorCode::ATA_IDNF);
		if (err & ATA_ERROR_MC)
			return BAN::Error::from_error_code(ErrorCode::ATA_MC);
		if (err & ATA_ERROR_UNC)
			return BAN::Error::from_error_code(ErrorCode::ATA_UNC);
		if (err & ATA_ERROR_BBK)
			return BAN::Error::from_error_code(ErrorCode::ATA_BBK);

		return BAN::Error::from_error_code(ErrorCode::None);
	}

	BAN::ErrorOr<void> ATABus::read(ATADevice& device, uint64_t lba, uint64_t sector_count, BAN::ByteSpan buffer)
	{
		ASSERT(sector_count <= 0xFF);
		ASSERT(buffer.size() >= sector_count * device.sector_size());
		if (lba + sector_count > device.sector_count())
			return BAN::Error::from_error_code(ErrorCode::Storage_Boundaries);

		LockGuard _(m_mutex);

		if (lba < (1 << 28))
		{
			// LBA28
			io_write(ATA_PORT_DRIVE_SELECT, 0xE0 | ((uint8_t)device.is_secondary() << 4) | ((lba >> 24) & 0x0F));
			select_delay();
			io_write(ATA_PORT_CONTROL, 0);

			io_write(ATA_PORT_SECTOR_COUNT, sector_count);
			io_write(ATA_PORT_LBA0, (uint8_t)(lba >>  0));
			io_write(ATA_PORT_LBA1, (uint8_t)(lba >>  8));
			io_write(ATA_PORT_LBA2, (uint8_t)(lba >> 16));
			io_write(ATA_PORT_COMMAND, ATA_COMMAND_READ_SECTORS);

			for (uint32_t sector = 0; sector < sector_count; sector++)
			{
				block_until_irq();
				read_buffer(ATA_PORT_DATA, (uint16_t*)buffer.data() + sector * device.words_per_sector(), device.words_per_sector());
			}
		}
		else
		{
			// LBA48
			ASSERT(false);
		}

		return {};
	}

	BAN::ErrorOr<void> ATABus::write(ATADevice& device, uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan buffer)
	{
		ASSERT(sector_count <= 0xFF);
		ASSERT(buffer.size() >= sector_count * device.sector_size());
		if (lba + sector_count > device.sector_count())
			return BAN::Error::from_error_code(ErrorCode::Storage_Boundaries);

		LockGuard _(m_mutex);

		if (lba < (1 << 28))
		{
			// LBA28
			io_write(ATA_PORT_DRIVE_SELECT, 0xE0 | ((uint8_t)device.is_secondary() << 4) | ((lba >> 24) & 0x0F));
			select_delay();
			io_write(ATA_PORT_CONTROL, 0);

			io_write(ATA_PORT_SECTOR_COUNT, sector_count);
			io_write(ATA_PORT_LBA0, (uint8_t)(lba >>  0));
			io_write(ATA_PORT_LBA1, (uint8_t)(lba >>  8));
			io_write(ATA_PORT_LBA2, (uint8_t)(lba >> 16));
			io_write(ATA_PORT_COMMAND, ATA_COMMAND_WRITE_SECTORS);

			for (uint32_t sector = 0; sector < sector_count; sector++)
			{
				write_buffer(ATA_PORT_DATA, (uint16_t*)buffer.data() + sector * device.words_per_sector(), device.words_per_sector());
				block_until_irq();
			}
		}
		else
		{
			// LBA48
			ASSERT(false);
		}

		io_write(ATA_PORT_COMMAND, ATA_COMMAND_CACHE_FLUSH);
		block_until_irq();

		return {};
	}

}
