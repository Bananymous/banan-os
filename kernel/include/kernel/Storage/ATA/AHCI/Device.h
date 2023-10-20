#pragma once

#include <kernel/Semaphore.h>
#include <kernel/Storage/ATA/AHCI/Definitions.h>
#include <kernel/Storage/ATA/ATADevice.h>

namespace Kernel
{

	class AHCIDevice final : public detail::ATABaseDevice
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<AHCIDevice>> create(BAN::RefPtr<AHCIController>, volatile HBAPortMemorySpace*);
		~AHCIDevice() = default;

	private:
		AHCIDevice(BAN::RefPtr<AHCIController> controller, volatile HBAPortMemorySpace* port)
			: m_controller(controller)
			, m_port(port)
		{ }
		BAN::ErrorOr<void> initialize();
		BAN::ErrorOr<void> allocate_buffers();
		BAN::ErrorOr<void> rebase();
		BAN::ErrorOr<void> read_identify_data();

		virtual BAN::ErrorOr<void> read_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ByteSpan) override;
		virtual BAN::ErrorOr<void> write_sectors_impl(uint64_t lba, uint64_t sector_count, BAN::ConstByteSpan) override;
		BAN::ErrorOr<void> send_command_and_block(uint64_t lba, uint64_t sector_count, Command command);

		BAN::Optional<uint32_t> find_free_command_slot();

		void handle_irq();
		void block_until_irq();

	private:
		BAN::RefPtr<AHCIController> m_controller;
		volatile HBAPortMemorySpace* const m_port;
		
		BAN::UniqPtr<DMARegion> m_dma_region;
		// Intermediate read/write buffer
		// TODO: can we read straight to user buffer?
		BAN::UniqPtr<DMARegion> m_data_dma_region;

		volatile bool m_has_got_irq { false };

		friend class AHCIController;
	};

}