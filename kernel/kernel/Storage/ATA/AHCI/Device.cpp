#include <kernel/Storage/ATA/AHCI/Controller.h>
#include <kernel/Storage/ATA/AHCI/Device.h>
#include <kernel/Storage/ATA/ATADefinitions.h>
#include <kernel/Thread.h>
#include <kernel/Timer/Timer.h>

namespace Kernel
{

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

	BAN::ErrorOr<BAN::RefPtr<AHCIDevice>> AHCIDevice::create(BAN::RefPtr<AHCIController> controller, volatile HBAPortMemorySpace* port)
	{
		auto* device_ptr = new AHCIDevice(controller, port);
		if (device_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		return BAN::RefPtr<AHCIDevice>::adopt(device_ptr);
	}

	BAN::ErrorOr<void> AHCIDevice::initialize()
	{
		TRY(allocate_buffers());
		TRY(rebase());

		// enable interrupts
		m_port->ie = 0xFFFFFFFF;

		TRY(read_identify_data());
		TRY(detail::ATABaseDevice::initialize({ (const uint16_t*)m_data_dma_region->vaddr(), m_data_dma_region->size() }));

		return {};
	}

	BAN::ErrorOr<void> AHCIDevice::allocate_buffers()
	{
		uint32_t command_slot_count = m_controller->command_slot_count();
		size_t needed_bytes = (sizeof(HBACommandHeader) + sizeof(HBACommandTable)) * command_slot_count + sizeof(ReceivedFIS);

		m_dma_region = TRY(DMARegion::create(needed_bytes));
		memset((void*)m_dma_region->vaddr(), 0x00, m_dma_region->size());

		m_data_dma_region = TRY(DMARegion::create(PAGE_SIZE));
		memset((void*)m_data_dma_region->vaddr(), 0x00, m_data_dma_region->size());
		
		return {};
	}

	BAN::ErrorOr<void> AHCIDevice::rebase()
	{
		ASSERT(m_dma_region);

		uint32_t command_slot_count = m_controller->command_slot_count();

		stop_cmd(m_port);

		paddr_t fis_paddr = m_dma_region->paddr();
		m_port->fb = fis_paddr & 0xFFFFFFFF;
		m_port->fbu = fis_paddr >> 32;

		paddr_t command_list_paddr = fis_paddr + sizeof(ReceivedFIS);
		m_port->clb = command_list_paddr & 0xFFFFFFFF;
		m_port->clbu = command_list_paddr >> 32;

		auto* command_headers = (HBACommandHeader*)m_dma_region->paddr_to_vaddr(command_list_paddr);
		paddr_t command_table_paddr = command_list_paddr + command_slot_count * sizeof(HBACommandHeader);
		for (uint32_t i = 0; i < command_slot_count; i++)
		{
			uint64_t command_table_entry_paddr = command_table_paddr + i * sizeof(HBACommandTable);
			command_headers[i].prdtl = s_hba_prdt_count;
			command_headers[i].ctba = command_table_entry_paddr & 0xFFFFFFFF;
			command_headers[i].ctbau = command_table_entry_paddr >> 32;
		}

		start_cmd(m_port);

		return {};
	}

	BAN::ErrorOr<void> AHCIDevice::read_identify_data()
	{
		ASSERT(m_data_dma_region);

		m_port->is = ~(uint32_t)0;

		auto slot = find_free_command_slot();
		ASSERT(slot.has_value());

		auto& command_header = ((HBACommandHeader*)m_dma_region->paddr_to_vaddr(m_port->clb))[slot.value()];
		command_header.cfl = sizeof(FISRegisterH2D) / sizeof(uint32_t);
		command_header.w = 0;
		command_header.prdtl = 1;

		auto& command_table = *(HBACommandTable*)m_dma_region->paddr_to_vaddr(command_header.ctba);
		memset(&command_table, 0x00, sizeof(HBACommandTable));
		command_table.prdt_entry[0].dba = m_data_dma_region->paddr();
		command_table.prdt_entry[0].dbc = 511;
		command_table.prdt_entry[0].i = 1;

		auto& command = *(FISRegisterH2D*)command_table.cfis;
		command.fis_type = FIS_TYPE_REGISTER_H2D;
		command.c = 1;
		command.command = ATA_COMMAND_IDENTIFY;

		while (m_port->tfd & (ATA_STATUS_BSY | ATA_STATUS_DRQ))
			continue;

		m_port->ci = 1 << slot.value();

		// FIXME: timeout
		do { block_until_irq(); } while (m_port->ci & (1 << slot.value()));

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
		ASSERT(!m_has_got_irq);
		uint16_t err = m_port->serr & 0xFFFF;
		if (err)
			print_error(err);
		m_has_got_irq = true;
	}

	void AHCIDevice::block_until_irq()
	{
		while (!__sync_bool_compare_and_swap(&m_has_got_irq, true, false))
			__builtin_ia32_pause();
	}

	BAN::ErrorOr<void> AHCIDevice::read_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * sector_size());
		const size_t sectors_per_page = PAGE_SIZE / sector_size();
		for (uint64_t sector_off = 0; sector_off < sector_count; sector_off += sectors_per_page)
		{
			uint64_t to_read = BAN::Math::min<uint64_t>(sector_count - sector_off, sectors_per_page);

			TRY(send_command_and_block(lba + sector_off, to_read, Command::Read));
			memcpy(buffer.data() + sector_off * sector_size(), (void*)m_data_dma_region->vaddr(), to_read * sector_size());
		}

		return {};
	}

	BAN::ErrorOr<void> AHCIDevice::write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan buffer)
	{
		ASSERT(buffer.size() >= sector_count * sector_size());
		const size_t sectors_per_page = PAGE_SIZE / sector_size();
		for (uint64_t sector_off = 0; sector_off < sector_count; sector_off += sectors_per_page)
		{
			uint64_t to_read = BAN::Math::min<uint64_t>(sector_count - sector_off, sectors_per_page);

			memcpy((void*)m_data_dma_region->vaddr(), buffer.data() + sector_off * sector_size(), to_read * sector_size());
			TRY(send_command_and_block(lba + sector_off, to_read, Command::Write));
		}

		return {};
	}

	BAN::ErrorOr<void> AHCIDevice::send_command_and_block(uint64_t lba, uint64_t sector_count, Command command)
	{
		ASSERT(m_dma_region);
		ASSERT(m_data_dma_region);
		ASSERT(0 < sector_count && sector_count <= 0xFFFF + 1);
		
		ASSERT(sector_count * sector_size() <= m_data_dma_region->size());

		m_port->is = ~(uint32_t)0;

		auto slot = find_free_command_slot();
		ASSERT(slot.has_value());
		
		auto& command_header = ((HBACommandHeader*)m_dma_region->paddr_to_vaddr(m_port->clb))[slot.value()];
		command_header.cfl = sizeof(FISRegisterH2D) / sizeof(uint32_t);
		command_header.prdtl = 1;
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

		auto& command_table = *(HBACommandTable*)m_dma_region->paddr_to_vaddr(command_header.ctba);
		memset(&command_table, 0x00, sizeof(HBACommandTable));
		command_table.prdt_entry[0].dba = m_data_dma_region->paddr() & 0xFFFFFFFF;
		command_table.prdt_entry[0].dbau = m_data_dma_region->paddr() >> 32;
		command_table.prdt_entry[0].dbc = sector_count * sector_size() - 1;
		command_table.prdt_entry[0].i = 1;

		auto& fis_command = *(FISRegisterH2D*)command_table.cfis;
		memset(&fis_command, 0x00, sizeof(FISRegisterH2D));
		fis_command.fis_type = FIS_TYPE_REGISTER_H2D;
		fis_command.c = 1;

		bool need_extended = lba >= (1 << 28) || sector_count > 0xFF;
		ASSERT (!need_extended || (m_command_set & ATA_COMMANDSET_LBA48_SUPPORTED));

		switch (command)
		{
			case Command::Read:
				fis_command.command = need_extended ? ATA_COMMAND_READ_DMA_EXT : ATA_COMMAND_READ_DMA;
				break;
			case Command::Write:
				fis_command.command = need_extended ? ATA_COMMAND_WRITE_DMA_EXT : ATA_COMMAND_WRITE_DMA;
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

		while (m_port->tfd & (ATA_STATUS_BSY | ATA_STATUS_DRQ))
			continue;

		m_port->ci = 1 << slot.value();

		// FIXME: timeout
		do { block_until_irq(); } while (m_port->ci & (1 << slot.value()));

		return {};
	}

	BAN::Optional<uint32_t> AHCIDevice::find_free_command_slot()
	{
		uint32_t slots = m_port->sact | m_port->ci;
		for (uint32_t i = 0; i < m_controller->command_slot_count(); i++, slots >>= 1)
			if (!(slots & 1))
				return i;
		return {};
	}

}
