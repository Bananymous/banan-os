#include <kernel/Lock/BlockableSpinLock.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Scheduler.h>
#include <kernel/Storage/ATA/AHCI/Controller.h>
#include <kernel/Storage/ATA/AHCI/Device.h>
#include <kernel/Storage/ATA/ATADefinitions.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

	static constexpr uint64_t s_ata_timeout_ms = 1000;

	static constexpr size_t align_up_to(size_t value, size_t alignment)
	{
		if (const size_t rem = value % alignment)
			value += alignment - rem;
		return value;
	}

	static void start_cmd(volatile HBAPortMemorySpace* port)
	{
		while (port->cmd & HBA_PxCMD_CR)
			continue;
		port->cmd = port->cmd | HBA_PxCMD_FRE;
		port->cmd = port->cmd | HBA_PxCMD_ST;
	}

	static void stop_cmd(volatile HBAPortMemorySpace* port)
	{
		port->cmd = port->cmd & ~HBA_PxCMD_ST;
		port->cmd = port->cmd & ~HBA_PxCMD_FRE;
		while (port->cmd & (HBA_PxCMD_FR | HBA_PxCMD_CR))
			continue;
	}

	paddr_t AHCIDevice::read_paddr(volatile uint32_t& lo, volatile uint32_t& hi) const
	{
		if (!m_controller->supports_64bit())
			return lo;
		return (static_cast<uint64_t>(hi) << 32) | lo;
	}

	void AHCIDevice::write_paddr(volatile uint32_t& lo, volatile uint32_t& hi, paddr_t paddr)
	{
		if (!m_controller->supports_64bit())
		{
			ASSERT((paddr >> 32) == 0);
			lo = paddr;
		}
		else
		{
			lo = paddr & 0xFFFFFFFF;
			hi = paddr >> 32;
		}
	}

	BAN::ErrorOr<BAN::RefPtr<AHCIDevice>> AHCIDevice::create(BAN::RefPtr<AHCIController> controller, volatile HBAPortMemorySpace* port)
	{
		auto* device_ptr = new AHCIDevice(controller, port);
		if (device_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<AHCIDevice>::adopt(device_ptr);
	}

	BAN::ErrorOr<void> AHCIDevice::initialize()
	{
		TRY(rebase());

		m_temp_buffer = TRY(DMARegion::create(256 * 1024, PageTable::MemoryType::Normal));
		memset(reinterpret_cast<void*>(m_temp_buffer->vaddr()), 0x00, m_temp_buffer->size());
		if (!m_controller->supports_64bit() && m_temp_buffer->paddr() + m_temp_buffer->size() > 0x100000000)
		{
			dwarnln("cannot allocate 32 bit buffer and the controller does not support 64 bit");
			return BAN::Error::from_errno(EFAULT);
		}

		if (const uint32_t command_slots = m_controller->command_slot_count(); command_slots < 32)
			m_free_slots = (1u << command_slots) - 1;
		else
			m_free_slots = -1;

		// enable interrupts
		m_port->ie = 0xFFFFFFFF;

		TRY(read_identify_data());
		TRY(detail::ATABaseDevice::initialize({ reinterpret_cast<const uint16_t*>(m_temp_buffer->vaddr()), 256 }));

		return {};
	}

	BAN::ErrorOr<void> AHCIDevice::rebase()
	{
		const uint32_t command_slot_count = m_controller->command_slot_count();

		const size_t command_list_size = command_slot_count * sizeof(HBACommandHeader);

		const size_t command_table_offset = align_up_to(command_list_size, 128);
		const size_t command_table_entry_size = align_up_to(sizeof(HBACommandTable) + m_max_hba_prdt_count * sizeof(HBAPRDTEntry), 128);

		const size_t fis_offset = align_up_to(command_table_offset + command_slot_count * command_table_entry_size, 256);

		m_dma_region = TRY(DMARegion::create(fis_offset + sizeof(ReceivedFIS)));
		if (!m_controller->supports_64bit() && m_dma_region->paddr() + m_dma_region->size() > 0x100000000)
		{
			dwarnln("cannot allocate 32 bit buffer and the controller does not support 64 bit");
			return BAN::Error::from_errno(EFAULT);
		}

		memset(reinterpret_cast<void*>(m_dma_region->vaddr()), 0x00, m_dma_region->size());

		stop_cmd(m_port);

		const paddr_t command_list_paddr = m_dma_region->paddr();
		write_paddr(m_port->clb, m_port->clbu, command_list_paddr);

		volatile auto* command_list = reinterpret_cast<volatile HBACommandHeader*>(m_dma_region->paddr_to_vaddr(command_list_paddr));
		const paddr_t command_table_base = command_list_paddr + command_slot_count * sizeof(HBACommandHeader);
		for (uint32_t slot = 0; slot < command_slot_count; slot++)
			write_paddr(command_list[slot].ctba, command_list[slot].ctbau, command_table_base + slot * command_table_entry_size);

		write_paddr(m_port->fb, m_port->fbu, m_dma_region->paddr() + fis_offset);

		start_cmd(m_port);

		return {};
	}

	BAN::ErrorOr<void> AHCIDevice::read_identify_data()
	{
		const auto slot = TRY(find_free_command_slot());

		const vaddr_t command_header_vaddr = m_dma_region->paddr_to_vaddr(read_paddr(m_port->clb, m_port->clbu));
		volatile auto& command_header = reinterpret_cast<volatile HBACommandHeader*>(command_header_vaddr)[slot];
		command_header.cfl   = sizeof(FISRegisterH2D) / sizeof(uint32_t);
		command_header.w     = 0;
		command_header.prdtl = 1;

		const vaddr_t command_table_vaddr = m_dma_region->paddr_to_vaddr(read_paddr(command_header.ctba, command_header.ctbau));
		volatile auto& command_table = *reinterpret_cast<volatile HBACommandTable*>(command_table_vaddr);
		write_paddr(command_table.prdt_entry[0].dba, command_table.prdt_entry[0].dbau, m_temp_buffer->paddr());
		command_table.prdt_entry[0].dbc = 511;
		command_table.prdt_entry[0].i   = 1;

		volatile auto& command = *reinterpret_cast<volatile FISRegisterH2D*>(&command_table.cfis[0]);
		command.fis_type = FIS_TYPE_REGISTER_H2D;
		command.c        = 1;
		command.command  = ATA_COMMAND_IDENTIFY;

		TRY(send_command_and_wait(slot));

		return {};
	}

	static void print_error(uint16_t error)
	{
		dprintln("Disk error:");
		if (error & (1 << 11))
			dprintln("  Internal Error");
		if (error & (1 << 10))
			dprintln("  Protocol Error");
		if (error & (1 << 9))
			dprintln("  Persistent Communication or Data Integrity Error");
		if (error & (1 << 8))
			dprintln("  Transient Data Integrity Error");
		if (error & (1 << 1))
			dprintln("  Recovered Communications Error");
		if (error & (1 << 0))
			dprintln("  Recovered Data Integrity Error");
	}

	void AHCIDevice::handle_irq()
	{
		while (const uint32_t is = m_port->is)
		{
			m_prev_is |= is;
			m_port->is = is;

			SpinLockGuard _(m_command_lock);
			m_command_blocker.unblock();
		}

		if (const uint32_t serr = m_port->serr & 0xFFFF)
		{
			m_port->serr = serr;
			print_error(serr);
		}
	}

	bool AHCIDevice::can_use_buffer_directly(BAN::ConstByteSpan buffer) const
	{
		const vaddr_t buffer_vaddr = reinterpret_cast<vaddr_t>(buffer.data());
		if (buffer_vaddr % 2)
			return false;

		if (m_controller->supports_64bit())
			return true;

		const vaddr_t buffer_base = buffer_vaddr & PAGE_ADDR_MASK;
		for (size_t off = 0; off < buffer.size(); off++)
			if (PageTable::kernel().physical_address_of(buffer_base + off) >= 0x100000000)
				return false;

		return true;
	}

	BAN::ErrorOr<void> AHCIDevice::read_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * sector_size());
		if (buffer.size() > sector_count * sector_size())
			buffer = buffer.slice(0, sector_count * sector_size());

		if (can_use_buffer_directly(buffer))
		{
			size_t sectors_done = 0;
			while (sectors_done < sector_count)
				sectors_done += TRY(send_command_sync(lba + sectors_done, buffer.slice(sectors_done * sector_size()), Command::Read));
		}
		else
		{
			LockGuard _(m_temp_buffer_mutex);

			uint8_t* const temp_buffer = reinterpret_cast<uint8_t*>(m_temp_buffer->vaddr());
			while (!buffer.empty())
			{
				const size_t max_bytes = BAN::Math::min(buffer.size(), m_temp_buffer->size());
				const size_t sectors = TRY(send_command_sync(lba, { temp_buffer, max_bytes }, Command::Read));
				memcpy(buffer.data(), temp_buffer, sectors * sector_size());
				buffer = buffer.slice(sectors * sector_size());
				lba += sectors;
			}
		}

		return {};
	}

	BAN::ErrorOr<void> AHCIDevice::write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * sector_size());
		if (buffer.size() > sector_count * sector_size())
			buffer = buffer.slice(0, sector_count * sector_size());

		if (can_use_buffer_directly(buffer))
		{
			size_t sectors_done = 0;
			while (sectors_done < sector_count)
				sectors_done += TRY(send_command_sync(lba + sectors_done, buffer.slice(sectors_done * sector_size()), Command::Write));
		}
		else
		{
			LockGuard _(m_temp_buffer_mutex);

			uint8_t* const temp_buffer = reinterpret_cast<uint8_t*>(m_temp_buffer->vaddr());
			while (!buffer.empty())
			{
				const size_t max_bytes = BAN::Math::min(buffer.size(), m_temp_buffer->size());
				memcpy(temp_buffer, buffer.data(), max_bytes);
				const size_t sectors = TRY(send_command_sync(lba, { temp_buffer, max_bytes }, Command::Write));
				buffer = buffer.slice(sectors * sector_size());
				lba += sectors;
			}
		}

		return {};
	}

	BAN::ErrorOr<size_t> AHCIDevice::send_command_sync(uint64_t lba, BAN::ConstByteSpan buffer, Command command)
	{
		ASSERT(m_dma_region);

		const auto slot = TRY(find_free_command_slot());

		const vaddr_t command_header_vaddr = m_dma_region->paddr_to_vaddr(read_paddr(m_port->clb, m_port->clbu));
		volatile auto& command_header = reinterpret_cast<volatile HBACommandHeader*>(command_header_vaddr)[slot];
		command_header.cfl = sizeof(FISRegisterH2D) / sizeof(uint32_t);
		switch (command)
		{
			case Command::Read:
				command_header.w = 0;
				break;
			case Command::Write:
				command_header.w = 1;
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		const vaddr_t command_table_vaddr = m_dma_region->paddr_to_vaddr(read_paddr(command_header.ctba, command_header.ctbau));
		volatile auto& command_table = *reinterpret_cast<volatile HBACommandTable*>(command_table_vaddr);

		size_t prdt_count = 0;
		size_t total_bytes = 0;
		paddr_t extend_paddr = 0;

		while (!buffer.empty())
		{
			const auto to_paddr = [](vaddr_t vaddr) -> paddr_t {
				return PageTable::kernel().physical_address_of(vaddr & PAGE_ADDR_MASK) + (vaddr % PAGE_SIZE);
			};

			const vaddr_t buffer_vaddr = reinterpret_cast<vaddr_t>(buffer.data());
			const paddr_t buffer_paddr = to_paddr(buffer_vaddr);

			const size_t bytes = BAN::Math::min(buffer.size(), PAGE_SIZE - buffer_vaddr % PAGE_SIZE);

			bool can_extend = true;
			if (prdt_count == 0)
				can_extend = false;
			else if (buffer_paddr != extend_paddr)
				can_extend = false;
			else if (command_table.prdt_entry[prdt_count - 1].dbc + bytes >= 0x400000)
				can_extend = false;

			if (can_extend)
				command_table.prdt_entry[prdt_count - 1].dbc += bytes;
			else
			{
				if (prdt_count >= m_max_hba_prdt_count)
					break;
				command_table.prdt_entry[prdt_count].dba  = buffer_paddr & 0xFFFFFFFF;
				command_table.prdt_entry[prdt_count].dbau = buffer_paddr >> 32;
				command_table.prdt_entry[prdt_count].dbc  = bytes - 1;
				prdt_count++;
			}

			buffer = buffer.slice(bytes);
			total_bytes += bytes;
			extend_paddr = buffer_paddr + bytes;
		}

		if (const size_t rem = total_bytes % sector_size())
		{
			// TODO: this wont work with block sizes > PAGE_SIZE
			ASSERT(rem < static_cast<size_t>(command_table.prdt_entry[prdt_count - 1].dbc + 1));
			command_table.prdt_entry[prdt_count - 1].dbc -= rem;
			total_bytes -= rem;
		}

		command_header.prdtl = prdt_count;

		const size_t sector_count = total_bytes / sector_size();

		volatile auto& fis_command = *reinterpret_cast<volatile FISRegisterH2D*>(&command_table.cfis[0]);
		fis_command.fis_type = FIS_TYPE_REGISTER_H2D;
		fis_command.c        = 1;

		const bool needs_extended = (lba + sector_count) > (1 << 24) || sector_count > 0xFF;
		ASSERT (!needs_extended || (m_command_set & ATA_COMMANDSET_LBA48_SUPPORTED));

		switch (command)
		{
			case Command::Read:
				fis_command.command = needs_extended ? ATA_COMMAND_READ_DMA_EXT : ATA_COMMAND_READ_DMA;
				break;
			case Command::Write:
				fis_command.command = needs_extended ? ATA_COMMAND_WRITE_DMA_EXT : ATA_COMMAND_WRITE_DMA;
				break;
			default:
				ASSERT_NOT_REACHED();
		}

		fis_command.lba0 = (lba >>  0) & 0xFF;
		fis_command.lba1 = (lba >>  8) & 0xFF;
		fis_command.lba2 = (lba >> 16) & 0xFF;
		fis_command.device = 1 << 6;	// LBA mode

		fis_command.lba3 = (lba >> 24) & 0xFF;
		fis_command.lba4 = (lba >> 32) & 0xFF;
		fis_command.lba5 = (lba >> 40) & 0xFF;

		fis_command.count_lo = (sector_count >> 0) & 0xFF;
		fis_command.count_hi = (sector_count >> 8) & 0xFF;

		TRY(send_command_and_wait(slot));

		return sector_count;
	}

	BAN::ErrorOr<void> AHCIDevice::send_command_and_wait(uint32_t slot)
	{
		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + s_ata_timeout_ms;

		SpinLockGuard _(m_command_lock);

		m_port->ci |= 1u << slot;

		while (m_port->ci & (1u << slot))
		{
			if (SystemTimer::get().ms_since_boot() >= timeout_ms)
			{
				m_free_slots |= 1u << slot;
				return BAN::Error::from_errno(ETIMEDOUT);
			}
			BlockableSpinLock block(m_command_lock);
			m_command_blocker.block_with_wake_time_ms(timeout_ms, &block);
		}

		m_free_slots |= 1u << slot;

		constexpr uint32_t is_error =
			(1 << 30) | // task file error
			(1 << 29) | // host bus fatal error
			(1 << 28) | // host bus data error
			(1 << 27) | // interface fatal error
			(1 << 26) | // interface non-fatal error
			(1 << 24);  // overflow

		if (const uint32_t is = (m_prev_is.exchange(0) | m_port->is); is & is_error)
			return BAN::Error::from_errno(EFAULT);

		if (m_port->tfd & (ATA_STATUS_ERR | ATA_STATUS_DF))
			return BAN::Error::from_errno(EFAULT);

		return {};
	}

	BAN::ErrorOr<uint32_t> AHCIDevice::find_free_command_slot()
	{
		const uint64_t timeout_ms = SystemTimer::get().ms_since_boot() + s_ata_timeout_ms;

		SpinLockGuard _(m_command_lock);

		for (;;)
		{
			if (const uint32_t usable_slots = ~(m_port->sact | m_port->ci) & m_free_slots)
			{
				const uint32_t slot = __builtin_ctz(usable_slots);
				m_free_slots &= ~(1u << slot);
				return slot;
			}

			if (SystemTimer::get().ms_since_boot() >= timeout_ms)
				return BAN::Error::from_errno(ETIMEDOUT);

			BlockableSpinLock block(m_command_lock);
			m_command_blocker.block_with_timeout_ms(timeout_ms, &block);
		}
	}

}
