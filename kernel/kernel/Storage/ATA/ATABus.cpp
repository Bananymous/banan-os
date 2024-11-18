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
		if (bus->io_read(ATA_PORT_STATUS) == 0xFF)
		{
			dprintln("Floating ATA bus on IO port 0x{H}", base);
			return BAN::Error::from_errno(ENODEV);
		}
		bus->set_irq(irq);
		InterruptController::get().enable_irq(irq);
		TRY(bus->initialize());
		return bus;
	}

	BAN::ErrorOr<void> ATABus::initialize()
	{
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
		SystemTimer::get().sleep_ns(400);
	}

	void ATABus::select_device(bool is_secondary)
	{
		io_write(ATA_PORT_DRIVE_SELECT, 0xA0 | ((uint8_t)is_secondary << 4));
		select_delay();
	}

	BAN::ErrorOr<ATABus::DeviceType> ATABus::identify(bool is_secondary, BAN::Span<uint16_t> buffer)
	{
		select_device(is_secondary);

		// Disable interrupts
		io_write(ATA_PORT_CONTROL, ATA_CONTROL_nIEN);

		io_write(ATA_PORT_SECTOR_COUNT, 0);
		io_write(ATA_PORT_LBA0, 0);
		io_write(ATA_PORT_LBA1, 0);
		io_write(ATA_PORT_LBA2, 0);
		io_write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY);
		SystemTimer::get().sleep_ms(1);

		// No device on port
		if (io_read(ATA_PORT_STATUS) == 0)
			return BAN::Error::from_errno(ENODEV);

		TRY(wait(false));

		const uint8_t lba1 = io_read(ATA_PORT_LBA1);
		const uint8_t lba2 = io_read(ATA_PORT_LBA2);

		auto device_type = DeviceType::ATA;
		if (lba1 || lba2)
		{
			if (lba1 == 0x14 && lba2 == 0xEB)
				device_type = DeviceType::ATAPI;
			else if (lba1 == 0x69 && lba2 == 0x96)
				device_type = DeviceType::ATAPI;
			else
			{
				dprintln("Unsupported device type {2H} {2H}", lba1, lba2);
				return BAN::Error::from_errno(EINVAL);
			}

			io_write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY_PACKET);
			SystemTimer::get().sleep_ms(1);
		}

		TRY(wait(true));

		ASSERT(buffer.size() >= 256);
		read_buffer(ATA_PORT_DATA, buffer.data(), 256);
		return device_type;
	}

	void ATABus::handle_irq()
	{
		if (io_read(ATA_PORT_STATUS) & ATA_STATUS_ERR)
			dprintln("ATA Error: {}", error());

		bool expected { false };
		[[maybe_unused]] bool success = m_has_got_irq.compare_exchange(expected, true);
		ASSERT(success);
	}

	BAN::ErrorOr<void> ATABus::block_until_irq()
	{
		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + s_ata_timeout_ms;

		bool expected { true };
		while (!m_has_got_irq.compare_exchange(expected, false))
		{
			if (SystemTimer::get().ms_since_boot() >= timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);
			Processor::pause();
			expected = true;
		}

		return {};
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

		TRY(send_command(device, lba, sector_count, false));

		for (uint32_t sector = 0; sector < sector_count; sector++)
		{
			TRY(block_until_irq());
			read_buffer(ATA_PORT_DATA, (uint16_t*)buffer.data() + sector * device.words_per_sector(), device.words_per_sector());
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

		TRY(send_command(device, lba, sector_count, true));

		for (uint32_t sector = 0; sector < sector_count; sector++)
		{
			write_buffer(ATA_PORT_DATA, (uint16_t*)buffer.data() + sector * device.words_per_sector(), device.words_per_sector());
			TRY(block_until_irq());
		}

		io_write(ATA_PORT_COMMAND, ATA_COMMAND_CACHE_FLUSH);
		TRY(block_until_irq());

		return {};
	}

	BAN::ErrorOr<void> ATABus::send_command(ATADevice& device, uint64_t lba, uint64_t sector_count, bool write)
	{
		uint8_t io_select = 0;
		uint8_t io_lba0 = 0;
		uint8_t io_lba1 = 0;
		uint8_t io_lba2 = 0;

		if (lba >= (1 << 28))
		{
			dwarnln("LBA48 addressing not supported");
			return BAN::Error::from_errno(ENOTSUP);
		}
		else if (device.has_lba())
		{
			io_select = 0xE0 | ((uint8_t)device.is_secondary() << 4) | ((lba >> 24) & 0x0F);
			io_lba0   = (lba >>  0) & 0xFF;
			io_lba1   = (lba >>  8) & 0xFF;
			io_lba2   = (lba >> 16) & 0xFF;
		}
		else
		{
			const uint8_t  sector   = (lba % 63) + 1;
			const uint8_t  head     = (lba + 1 - sector) % (16 * 63) / 63;
			const uint16_t cylinder = (lba + 1 - sector) / (16 * 63);

			io_select = 0xA0 | ((uint8_t)device.is_secondary() << 4) | head;
			io_lba0   = sector;
			io_lba1   = (cylinder >> 0) & 0xFF;
			io_lba2   = (cylinder >> 8) & 0xFF;
		}

		io_write(ATA_PORT_DRIVE_SELECT, io_select);
		select_delay();
		io_write(ATA_PORT_CONTROL, 0);

		io_write(ATA_PORT_SECTOR_COUNT, sector_count);
		io_write(ATA_PORT_LBA0, io_lba0);
		io_write(ATA_PORT_LBA1, io_lba1);
		io_write(ATA_PORT_LBA2, io_lba2);
		io_write(ATA_PORT_COMMAND, write ? ATA_COMMAND_WRITE_SECTORS : ATA_COMMAND_READ_SECTORS);

		return {};
	}

}
