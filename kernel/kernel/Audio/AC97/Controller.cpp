#include <kernel/Audio/AC97/Controller.h>
#include <kernel/Audio/AC97/Definitions.h>

namespace Kernel
{

	enum AudioMixerRegister : uint8_t
	{
		Reset = 0x00,
		MasterVolume = 0x02,
		AuxOutVolume = 0x04,
		MonoVolume = 0x06,
		MasterToneRL = 0x08,
		PC_BEEPVolume = 0x0A,
		PhoneVolume = 0x0C,
		MicVolume = 0x0E,
		LineInVolume = 0x10,
		CDVolume = 0x12,
		VideoVolume = 0x14,
		AuxInVolume = 0x16,
		PCMOutVolume = 0x18,
		RecordSelect = 0x1A,
		RecordGain = 0x1C,
		RecordGainMic = 0x1E,
		GeneralPurpose = 0x20,
		_3DControl = 0x22,
		PowerdownCtrlStat = 0x26,
		ExtendedAudio = 0x28,
		ExtendedAudioCtrlStat = 0x2A,
		PCMFrontDACRate = 0x2C,
		PCMSurroundDACRate = 0x2E,
		PCMLFEDACRate = 0x30,
		PCMLRADCRate = 0x32,
		MICADCRate = 0x34,
		_6ChVolC_LFE = 0x36,
		_6ChVolL_R_Surround = 0x38,
		S_PDIFControl = 0x3A
	};

	enum BusMasterRegister : uint8_t
	{
		PI_BDBAR = 0x00,
		PI_CIV = 0x04,
		PI_LVI = 0x05,
		PI_SR = 0x06,
		PI_PICB = 0x08,
		PI_PIV = 0x0A,
		PI_CR = 0x0B,
		PO_BDBAR = 0x10,
		PO_CIV = 0x14,
		PO_LVI = 0x15,
		PO_SR = 0x16,
		PO_PICB = 0x18,
		PO_PIV = 0x1A,
		PO_CR = 0x1B,
		MC_BDBAR = 0x20,
		MC_CIV = 0x24,
		MC_LVI = 0x25,
		MC_SR = 0x26,
		MC_PICB = 0x28,
		MC_PIV = 0x2A,
		MC_CR = 0x2B,
		GLOB_CNT = 0x2C,
		GLOB_STA = 0x30,
		CAS = 0x34,
		MC2_BDBAR = 0x40,
		MC2_CIV = 0x44,
		MC2_LVI = 0x45,
		MC2_SR = 0x46,
		MC2_PICB = 0x48,
		MC2_PIV = 0x4A,
		MC2_CR = 0x4B,
		PI2_BDBAR = 0x50,
		PI2_CIV = 0x54,
		PI2_LVI = 0x55,
		PI2_SR = 0x56,
		PI2_PICB = 0x58,
		PI2_PIV = 0x5A,
		PI2_CR = 0x5B,
		SPBAR = 0x60,
		SPCIV = 0x64,
		SPLVI = 0x65,
		SPSR = 0x66,
		SPPICB = 0x68,
		SPPIV = 0x6A,
		SPCR = 0x6B,
		SDM = 0x80,
	};

	enum BusMasterStatus : uint16_t
	{
		DMAHalted = 1 << 0,
		CELV      = 1 << 1,
		LVBCI     = 1 << 2,
		BCIS      = 1 << 3,
		FIFOE     = 1 << 4,
	};

	enum BusMasterControl : uint8_t
	{
		RDBM  = 1 << 0,
		RR    = 1 << 1,
		LVBIE = 1 << 2,
		FEIE  = 1 << 3,
		IOCE  = 1 << 4,
	};

	BAN::ErrorOr<BAN::RefPtr<AC97AudioController>> AC97AudioController::create(PCI::Device& pci_device)
	{
		auto* ac97_ptr = new AC97AudioController(pci_device);
		if (ac97_ptr == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		auto ac97 = BAN::RefPtr<AC97AudioController>::adopt(ac97_ptr);
		TRY(ac97->initialize());
		return ac97;
	}

	BAN::ErrorOr<void> AC97AudioController::initialize()
	{
		m_pci_device.enable_bus_mastering();

		m_mixer = TRY(m_pci_device.allocate_bar_region(0));
		m_bus_master = TRY(m_pci_device.allocate_bar_region(1));

		if (m_mixer->size() < 0x34 || m_bus_master->size() < 0x34)
			return BAN::Error::from_errno(ENODEV);

		// Reset bus master
		m_bus_master->write32(BusMasterRegister::GLOB_CNT, 0x00);

		// no interrupts, 2 channels, 16 bit samples
		m_bus_master->write32(BusMasterRegister::GLOB_CNT, 0x02);

		// Reset mixer to default values
		m_mixer->write16(AudioMixerRegister::Reset, 0);

		// Master volume 100%, no mute
		m_mixer->write16(AudioMixerRegister::MasterVolume, 0x0000);

		// PCM output volume left/right +0 db, no mute
		m_mixer->write16(AudioMixerRegister::PCMOutVolume, 0x0808);

		TRY(initialize_bld());

		TRY(initialize_interrupts());

		// disable transfer, enable all interrupts
		m_bus_master->write8(BusMasterRegister::PO_CR, IOCE | FEIE | LVBIE);

		return {};
	}

	BAN::ErrorOr<void> AC97AudioController::initialize_bld()
	{
		const size_t bdl_size = sizeof(AC97::BufferDescriptorListEntry) * m_bdl_entries;
		const size_t buffer_size = m_samples_per_entry * sizeof(int16_t);

		m_bdl_region = TRY(DMARegion::create(bdl_size + buffer_size * m_used_bdl_entries));
		memset(reinterpret_cast<void*>(m_bdl_region->vaddr()), 0x00, m_bdl_region->size());

		for (size_t i = 0; i < m_bdl_entries; i++)
		{
			auto& entry = reinterpret_cast<AC97::BufferDescriptorListEntry*>(m_bdl_region->vaddr())[i];
			entry.address = m_bdl_region->paddr() + bdl_size + (i % m_used_bdl_entries) * buffer_size;
			entry.samples = 0;
			entry.flags = 0;
		}

		m_bus_master->write32(BusMasterRegister::PO_BDBAR, m_bdl_region->paddr());

		return {};
	}

	BAN::ErrorOr<void> AC97AudioController::initialize_interrupts()
	{
		TRY(m_pci_device.reserve_interrupts(1));
		m_pci_device.enable_interrupt(0, *this);

		// enable interrupts
		m_bus_master->write32(BusMasterRegister::GLOB_CNT, m_bus_master->read32(BusMasterRegister::GLOB_CNT) | 0x01);

		return {};
	}

	void AC97AudioController::handle_new_data()
	{
		ASSERT(m_spinlock.current_processor_has_lock());
		queue_samples_to_bld();
	}

	bool AC97AudioController::queue_samples_to_bld()
	{
		ASSERT(m_spinlock.current_processor_has_lock());

		uint32_t lvi = m_bdl_head;

		while (m_bdl_head != (m_bdl_tail + m_used_bdl_entries) % m_bdl_entries)
		{
			const uint32_t next_bld_head = (m_bdl_head + 1) % m_bdl_entries;
			if (next_bld_head == m_bdl_tail)
				break;

			const size_t sample_data_tail = (m_sample_data_head + m_sample_data_capacity - m_sample_data_size) % m_sample_data_capacity;

			const size_t max_memcpy = BAN::Math::min(m_sample_data_size, m_sample_data_capacity - sample_data_tail);
			const size_t samples = BAN::Math::min(max_memcpy / 2, m_samples_per_entry);
			if (samples == 0)
				break;

			auto& entry = reinterpret_cast<AC97::BufferDescriptorListEntry*>(m_bdl_region->vaddr())[m_bdl_head];
			entry.samples = samples;
			entry.flags = (1 << 15);
			memcpy(
				reinterpret_cast<void*>(m_bdl_region->paddr_to_vaddr(entry.address)),
				&m_sample_data[sample_data_tail],
				samples * 2
			);

			m_sample_data_size -= samples * 2;

			lvi = m_bdl_head;
			m_bdl_head = next_bld_head;
		}

		// if head was not updated, no data was queued
		if (lvi == m_bdl_head)
			return false;

		m_sample_data_blocker.unblock();

		m_bus_master->write8(BusMasterRegister::PO_LVI, lvi);

		// start playing if we are not already
		const uint8_t control = m_bus_master->read8(BusMasterRegister::PO_CR);
		if (!(control & RDBM))
			m_bus_master->write8(BusMasterRegister::PO_CR, control | RDBM);

		return true;
	}

	void AC97AudioController::handle_irq()
	{
		const uint16_t status = m_bus_master->read16(BusMasterRegister::PO_SR);
		if (!(status & (LVBCI | BCIS | FIFOE)))
			return;
		m_bus_master->write16(BusMasterRegister::PO_SR, LVBCI | BCIS | FIFOE);

		SpinLockGuard _(m_spinlock);

		bool did_enqueue = false;
		if (status & BCIS)
		{
			m_bdl_tail = (m_bdl_tail + 1) % m_bdl_entries;
			did_enqueue = queue_samples_to_bld();
		}

		if ((status & LVBCI) && !did_enqueue)
		{
			const uint8_t control = m_bus_master->read8(BusMasterRegister::PO_CR);
			m_bus_master->write8(BusMasterRegister::PO_CR, control & ~RDBM);
		}
	}

}
