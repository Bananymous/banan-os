#pragma once

#include <kernel/Audio/Controller.h>
#include <kernel/Memory/DMARegion.h>

namespace Kernel
{

	class AC97AudioController : public AudioController, public Interruptable
	{
	public:
		static BAN::ErrorOr<BAN::RefPtr<AC97AudioController>> create(PCI::Device& pci_device);

		void handle_irq() override;

	protected:
		void handle_new_data() override;

		uint32_t get_channels() const override { return 2; }
		uint32_t get_sample_rate() const override { return 48000; }

	private:
		AC97AudioController(PCI::Device& pci_device)
			: m_pci_device(pci_device)
		{ }

		BAN::ErrorOr<void> initialize();
		BAN::ErrorOr<void> initialize_bld();
		BAN::ErrorOr<void> initialize_interrupts();

		bool queue_samples_to_bld();

	private:
		static constexpr size_t m_bdl_entries = 32;
		static constexpr size_t m_samples_per_entry = 0x1000;

		// We only store samples in 2 BDL entries at a time to reduce the amount of samples queued.
		// This is to reduce latency as you cannot remove data already passed to the BDLs
		static constexpr size_t m_used_bdl_entries = 2;

		PCI::Device& m_pci_device;
		BAN::UniqPtr<PCI::BarRegion> m_mixer;
		BAN::UniqPtr<PCI::BarRegion> m_bus_master;

		BAN::UniqPtr<DMARegion> m_bdl_region;

		uint32_t m_bdl_tail { 0 };
		uint32_t m_bdl_head { 0 };
	};

}
