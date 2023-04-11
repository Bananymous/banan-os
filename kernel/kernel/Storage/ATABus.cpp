#include <BAN/ScopeGuard.h>
#include <kernel/IDT.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/LockGuard.h>
#include <kernel/Storage/ATADevice.h>
#include <kernel/Storage/ATABus.h>
#include <kernel/Storage/ATADefinitions.h>

#include <kernel/CriticalScope.h>

namespace Kernel
{

	static void bus_irq_handler0();
	static void bus_irq_handler1();

	struct BusIRQ
	{
		ATABus* bus { nullptr };
		void (*handler)() { nullptr };
		uint8_t irq { 0 };
	};
	static BusIRQ s_bus_irqs[] {
		{ nullptr, bus_irq_handler0, 0 },
		{ nullptr, bus_irq_handler1, 0 },
	};

	static void bus_irq_handler0() { ASSERT(s_bus_irqs[0].bus); s_bus_irqs[0].bus->on_irq(); }
	static void bus_irq_handler1() { ASSERT(s_bus_irqs[1].bus); s_bus_irqs[1].bus->on_irq(); }

	static void register_bus_irq_handler(ATABus* bus, uint8_t irq)
	{
		for (uint8_t i = 0; i < sizeof(s_bus_irqs) / sizeof(BusIRQ); i++)
		{
			if (s_bus_irqs[i].bus == nullptr)
			{
				s_bus_irqs[i].bus = bus;
				s_bus_irqs[i].irq = irq;

				IDT::register_irq_handler(irq, s_bus_irqs[i].handler);
				InterruptController::get().enable_irq(irq);
				return;
			}
		}
		ASSERT_NOT_REACHED();
	}

	ATABus* ATABus::create(ATAController* controller, uint16_t base, uint16_t ctrl, uint8_t irq)
	{
		ATABus* bus = new ATABus(controller, base, ctrl);
		ASSERT(bus);
		bus->initialize(irq);
		return bus;
	}

	void ATABus::initialize(uint8_t irq)
	{
		register_bus_irq_handler(this, irq);

		uint16_t* identify_buffer = new uint16_t[256];
		ASSERT(identify_buffer);
		BAN::ScopeGuard _([identify_buffer] { delete[] identify_buffer; });

		for (uint8_t i = 0; i < 2; i++)
		{
			m_devices[i] = new ATADevice(this);
			ATADevice* device = m_devices[i];
			ASSERT(device);

			BAN::ScopeGuard guard([this, i] { m_devices[i]->unref(); m_devices[i] = nullptr; });

			
			auto type = identify(device, identify_buffer);
			if (type == DeviceType::None)
				continue;

			auto res = device->initialize(type, identify_buffer);
			if (res.is_error())
			{
				dprintln("{}", res.error());
				continue;
			}

			guard.disable();
		}

		// Enable disk interrupts
		for (int i = 0; i < 2; i++)
		{
			if (!m_devices[i])
				continue;
			select_device(m_devices[i]);
			io_write(ATA_PORT_CONTROL, 0);
		}
	}

	void ATABus::select_device(const ATADevice* device)
	{
		uint8_t device_index = this->device_index(device);
		io_write(ATA_PORT_DRIVE_SELECT, 0xA0 | (device_index << 4));
		PIT::sleep(1);
	}

	ATABus::DeviceType ATABus::identify(const ATADevice* device, uint16_t* buffer)
	{
		select_device(device);

		// Disable interrupts
		io_write(ATA_PORT_CONTROL, ATA_CONTROL_nIEN);

		io_write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY);
		PIT::sleep(1);
		
		// No device on port
		if (io_read(ATA_PORT_STATUS) == 0)
			return DeviceType::None;

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
				return DeviceType::None;
			}

			io_write(ATA_PORT_COMMAND, ATA_COMMAND_IDENTIFY_PACKET);
			PIT::sleep(1);

			if (auto res = wait(true); res.is_error())
			{
				dprintln("Fatal error: {}", res.error());
				return DeviceType::None;
			}
		}

		read_buffer(ATA_PORT_DATA, buffer, 256);

		return type;
	}

	void ATABus::on_irq()
	{
		ASSERT(!m_has_got_irq);
		if (io_read(ATA_PORT_STATUS) & ATA_STATUS_ERR)
			dprintln("ATA Error: {}", error());
		m_has_got_irq = true;
		m_semaphore.unblock();
	}

	void ATABus::block_until_irq()
	{
		if (!m_has_got_irq)
			m_semaphore.block();
		m_has_got_irq = false;
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

		uint8_t status = ATA_STATUS_BSY;
		while (status & ATA_STATUS_BSY)
			status = io_read(ATA_PORT_STATUS);

		while (wait_drq && !(status & ATA_STATUS_DRQ))
		{
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

	uint8_t ATABus::device_index(const ATADevice* device) const
	{
		ASSERT(device == m_devices[0] || device == m_devices[1]);
		return (device == m_devices[0]) ? 0 : 1;	
	}

	BAN::ErrorOr<void> ATABus::read(ATADevice* device, uint64_t lba, uint8_t sector_count, uint8_t* buffer)
	{
		if (lba + sector_count > device->m_lba_count)
			return BAN::Error::from_error_code(ErrorCode::Storage_Boundaries);

		LockGuard _(m_lock);

		if (lba < (1 << 28))
		{
			// LBA28
			io_write(ATA_PORT_DRIVE_SELECT, 0xE0 | (device_index(device) << 4) | ((lba >> 24) & 0x0F));
			io_write(ATA_PORT_SECTOR_COUNT, sector_count);
			io_write(ATA_PORT_LBA0, (uint8_t)(lba >>  0));
			io_write(ATA_PORT_LBA1, (uint8_t)(lba >>  8));
			io_write(ATA_PORT_LBA2, (uint8_t)(lba >> 16));
			io_write(ATA_PORT_COMMAND, ATA_COMMAND_READ_SECTORS);

			PIT::sleep(1);

			for (uint32_t sector = 0; sector < sector_count; sector++)
			{
				block_until_irq();
				read_buffer(ATA_PORT_DATA, (uint16_t*)buffer + sector * device->m_sector_words, device->m_sector_words);
			}
		}
		else
		{
			// LBA48
			ASSERT(false);
		}

		return {};
	}

	BAN::ErrorOr<void> ATABus::write(ATADevice* device, uint64_t lba, uint8_t sector_count, const uint8_t* buffer)
	{
		if (lba + sector_count > device->m_lba_count)
			return BAN::Error::from_error_code(ErrorCode::Storage_Boundaries);

		LockGuard _(m_lock);

		if (lba < (1 << 28))
		{
			// LBA28
			io_write(ATA_PORT_DRIVE_SELECT, 0xE0 | (device_index(device) << 4) | ((lba >> 24) & 0x0F));
			io_write(ATA_PORT_SECTOR_COUNT, sector_count);
			io_write(ATA_PORT_LBA0, (uint8_t)(lba >>  0));
			io_write(ATA_PORT_LBA1, (uint8_t)(lba >>  8));
			io_write(ATA_PORT_LBA2, (uint8_t)(lba >> 16));
			io_write(ATA_PORT_COMMAND, ATA_COMMAND_WRITE_SECTORS);

			PIT::sleep(1);

			for (uint32_t sector = 0; sector < sector_count; sector++)
			{
				write_buffer(ATA_PORT_DATA, (uint16_t*)buffer + sector * device->m_sector_words, device->m_sector_words);
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