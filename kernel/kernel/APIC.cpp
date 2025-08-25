#include <BAN/ScopeGuard.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/APIC.h>
#include <kernel/CPUID.h>
#include <kernel/Debug.h>
#include <kernel/IDT.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>
#include <kernel/Timer/Timer.h>

#include <string.h>

#define LAPIC_EIO_REG		0xB0
#define LAPIC_SIV_REG		0xF0
#define LAPIC_IS_REG		0x100
#define LAPIC_ERROR_REG		0x280
#define LAPIC_ICR_LO_REG	0x300
#define LAPIC_ICR_HI_REG	0x310

#define LAPIC_TIMER_LVT			0x320
#define LAPIC_TIMER_INITIAL_REG	0x380
#define LAPIC_TIMER_CURRENT_REG	0x390
#define LAPIC_TIMER_DIVIDE_REG	0x3E0

#define IOAPIC_MAX_REDIRS	0x01
#define IOAPIC_REDIRS		0x10

// https://uefi.org/specs/ACPI/6.5/05_ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt-format

extern uint8_t g_ap_init_addr[];

extern volatile uint8_t g_ap_startup_done[];
extern volatile uint8_t g_ap_running_count[];

namespace Kernel
{

	enum ICR_LO : uint32_t
	{
		ICR_LO_reserved_mask							= 0xFFF32000,

		ICR_LO_delivery_mode_fixed						= 0b000 << 8,
		ICR_LO_delivery_mode_lowest_priority			= 0b001 << 8,
		ICR_LO_delivery_mode_smi						= 0b010 << 8,
		ICR_LO_delivery_mode_nmi						= 0b100 << 8,
		ICR_LO_delivery_mode_init						= 0b101 << 8,
		ICR_LO_delivery_mode_start_up					= 0b110 << 8,

		ICR_LO_destination_mode_physical				= 0 << 11,
		ICR_LO_destination_mode_logical					= 1 << 11,

		ICR_LO_delivery_status_idle						= 0 << 12,
		ICR_LO_delivery_status_send_pending				= 1 << 12,

		ICR_LO_level_deassert							= 0 << 14,
		ICR_LO_level_assert								= 1 << 14,

		ICR_LO_trigger_mode_edge						= 0 << 15,
		ICR_LO_trigger_mode_level						= 1 << 15,

		ICR_LO_destination_shorthand_none				= 0b00 << 18,
		ICR_LO_destination_shorthand_self				= 0b01 << 18,
		ICR_LO_destination_shorthand_all_including_self	= 0b10 << 18,
		ICR_LO_destination_shorthand_all_excluding_self	= 0b11 << 18,
	};

	enum TimerLVT : uint32_t
	{
		TimerModeOneShot     = 0b00 << 17,
		TimerModePeriodic    = 0b01 << 17,
		TimerModeTSCDeadline = 0b10 << 17,

		TimerMask            = 1 << 16,
	};

	enum TimerDivideRegister : uint32_t
	{
		DivideBy2   = 0b0000,
		DivideBy4   = 0b0001,
		DivideBy8   = 0b0010,
		DivideBy16  = 0b0011,
		DivideBy32  = 0b1000,
		DivideBy64  = 0b1001,
		DivideBy128 = 0b1010,
		DivideBy1   = 0b1011,
	};

	struct MADT : public Kernel::ACPI::SDTHeader
	{
		uint32_t local_apic;
		uint32_t flags;
	} __attribute__((packed));

	struct MADTEntry
	{
		uint8_t type;
		uint8_t length;
		union
		{
			struct
			{
				uint8_t acpi_processor_id;
				uint8_t apic_id;
				uint32_t flags;
			} __attribute__((packed)) entry0;
			struct
			{
				uint8_t ioapic_id;
				uint8_t reserved;
				uint32_t ioapic_address;
				uint32_t gsi_base;
			} __attribute__((packed)) entry1;
			struct
			{
				uint8_t bus_source;
				uint8_t irq_source;
				uint32_t gsi;
				uint16_t flags;
			} __attribute__((packed)) entry2;
			struct
			{
				uint16_t reserved;
				uint64_t address;
			} __attribute__((packed)) entry5;
		};
	} __attribute__((packed));

	union RedirectionEntry
	{
		struct
		{
			uint64_t vector				: 8;
			uint64_t delivery_mode		: 3;
			uint64_t destination_mode	: 1;
			uint64_t delivery_status	: 1;
			uint64_t pin_polarity		: 1;
			uint64_t remote_irr			: 1;
			uint64_t trigger_mode		: 1;
			uint64_t mask				: 1;
			uint64_t reserved			: 39;
			uint64_t destination		: 8;
		};
		struct
		{
			uint32_t lo_dword;
			uint32_t hi_dword;
		};
	};

	APIC* APIC::create()
	{
		uint32_t ecx, edx;
		CPUID::get_features(ecx, edx);
		if (!(edx & CPUID::Features::EDX_APIC))
		{
			dprintln("Local APIC is not available");
			return nullptr;
		}

		const MADT* madt = (const MADT*)ACPI::ACPI::get().get_header("APIC"_sv, 0);
		if (madt == nullptr)
		{
			dprintln("Could not find MADT header");
			return nullptr;
		}

		APIC* apic = new APIC;
		apic->m_local_apic_paddr = madt->local_apic;
		for (uint32_t i = 0x00; i <= 0xFF; i++)
			apic->m_irq_overrides[i] = i;

		uintptr_t madt_entry_addr = (uintptr_t)madt + sizeof(MADT);
		while (madt_entry_addr < (uintptr_t)madt + madt->length)
		{
			const MADTEntry* entry = (const MADTEntry*)madt_entry_addr;
			switch (entry->type)
			{
				case 0:
					MUST(apic->m_processors.emplace_back(Processor {
						.processor_id = entry->entry0.acpi_processor_id,
						.apic_id      = entry->entry0.apic_id,
						.flags        = static_cast<uint8_t>(entry->entry0.flags & 0x03),
					}));
					break;
				case 1:
					MUST(apic->m_io_apics.emplace_back(IOAPIC {
						.id         = entry->entry1.ioapic_id,
						.paddr      = entry->entry1.ioapic_address,
						.vaddr      = 0,
						.gsi_base   = entry->entry1.gsi_base,
						.max_redirs = 0,
					}));
					break;
				case 2:
					apic->m_irq_overrides[entry->entry2.irq_source] = entry->entry2.gsi;
					break;
				case 5:
					apic->m_local_apic_paddr = entry->entry5.address;
					break;
				default:
					dprintln("Unhandled madt entry, type {}", entry->type);
					break;
			}
			madt_entry_addr += entry->length;
		}

		if (apic->m_local_apic_paddr == 0 || apic->m_io_apics.empty())
		{
			dprintln("MADT did not provide necessary information");
			delete apic;
			return nullptr;
		}

		// Map the local apic to kernel memory
		{
			vaddr_t vaddr = PageTable::kernel().reserve_free_page(KERNEL_OFFSET);
			ASSERT(vaddr);
			dprintln("lapic paddr {8H}", apic->m_local_apic_paddr);
			apic->m_local_apic_vaddr = vaddr + (apic->m_local_apic_paddr % PAGE_SIZE);
			dprintln("lapic vaddr {8H}", apic->m_local_apic_vaddr);
			PageTable::kernel().map_page_at(
				apic->m_local_apic_paddr & PAGE_ADDR_MASK,
				apic->m_local_apic_vaddr & PAGE_ADDR_MASK,
				PageTable::Flags::ReadWrite | PageTable::Flags::Present
			);
		}

		// Map io apics to kernel memory
		for (auto& io_apic : apic->m_io_apics)
		{
			vaddr_t vaddr = PageTable::kernel().reserve_free_page(KERNEL_OFFSET);
			ASSERT(vaddr);

			io_apic.vaddr = vaddr + (io_apic.paddr % PAGE_SIZE);

			PageTable::kernel().map_page_at(
				io_apic.paddr & PAGE_ADDR_MASK,
				io_apic.vaddr & PAGE_ADDR_MASK,
				PageTable::Flags::ReadWrite | PageTable::Flags::Present
			);
			io_apic.max_redirs = io_apic.read(IOAPIC_MAX_REDIRS);
		}

		// Enable local apic
		apic->write_to_local_apic(LAPIC_SIV_REG, apic->read_from_local_apic(LAPIC_SIV_REG) | 0x1FF);

		return apic;
	}

	void APIC::initialize_multiprocessor()
	{
		constexpr auto udelay = [](uint64_t us) { SystemTimer::get().pre_scheduler_sleep_ns(us * 1000); };

		const auto send_ipi =
			[&](uint8_t processor, uint32_t data, uint64_t ud)
			{
				while ((read_from_local_apic(LAPIC_ICR_LO_REG) & ICR_LO_delivery_status_send_pending) == ICR_LO_delivery_status_send_pending)
					__builtin_ia32_pause();
				write_to_local_apic(LAPIC_ICR_HI_REG, (read_from_local_apic(LAPIC_ICR_HI_REG) & 0x00FFFFFF) | (processor << 24));
				write_to_local_apic(LAPIC_ICR_LO_REG, data);
				udelay(ud);
			};

		dprintln("System has {} processors", m_processors.size());

		uint8_t bsp_id = Kernel::Processor::current_id().as_u32();
		dprintln("BSP lapic id: {}", bsp_id);

		if (m_processors.size() == 1)
		{
			dprintln("Only one processor, skipping AP initialization");
			*g_ap_startup_done = 1;
			return;
		}

		constexpr paddr_t ap_init_paddr = 0xF000;
		PageTable::with_fast_page(ap_init_paddr, [&] {
			memcpy(PageTable::fast_page_as_ptr(), g_ap_init_addr, PAGE_SIZE);
		});

		uint8_t initialized_aps = 0;
		for (auto& processor : m_processors)
		{
			if (processor.apic_id == bsp_id)
				continue;

			if (!(processor.flags & (Processor::Flags::Enabled | Processor::Flags::OnlineCapable)))
			{
				dwarnln("Skipping processor (lapic id {}) initialization", processor.apic_id);
				continue;
			}

			dprintln("Trying to enable processor (lapic id {})", processor.apic_id);

			auto& proc = Kernel::Processor::create(ProcessorID(processor.apic_id));
			PageTable::with_fast_page(ap_init_paddr, [&] {
				PageTable::fast_page_as_sized<uint32_t>(2) = kmalloc_paddr_of(proc.stack_top()).value();
				PageTable::fast_page_as_sized<uint8_t>(13) = 0;
			});

			write_to_local_apic(LAPIC_ERROR_REG, 0x00);

			// send INIT IPI
			send_ipi(processor.apic_id,
				(read_from_local_apic(LAPIC_ICR_LO_REG) & ICR_LO_reserved_mask)
				| ICR_LO_delivery_mode_init
				| ICR_LO_destination_mode_physical
				| ICR_LO_level_assert
				| ICR_LO_trigger_mode_edge
				| ICR_LO_destination_shorthand_none
				, 0
			);

			// TODO: If we are on processor predating Pentium, we need to send deassert

			udelay(10 * 1000);

			for (int i = 0; i < 2; i++)
			{
				write_to_local_apic(LAPIC_ERROR_REG, 0x00);

				// send 2 SETUP IPIs with 200 us delay
				send_ipi(processor.apic_id,
					(read_from_local_apic(LAPIC_ICR_LO_REG) & ICR_LO_reserved_mask)
					| ICR_LO_delivery_mode_start_up
					| ICR_LO_destination_mode_physical
					| ICR_LO_level_assert
					| ICR_LO_trigger_mode_edge
					| ICR_LO_destination_shorthand_none
					| (ap_init_paddr / PAGE_SIZE)
					, 200
				);
			}

			// give processor upto 100 * 100 us + 200 us to boot
			PageTable::with_fast_page(ap_init_paddr, [&] {
				for (int i = 0; i < 100; i++)
				{
					if (__atomic_load_n(&PageTable::fast_page_as_sized<uint8_t>(13), __ATOMIC_SEQ_CST))
						break;
					udelay(100);
				}
			});

			initialized_aps++;
		}

		__atomic_store_n(&g_ap_startup_done[0], 1, __ATOMIC_SEQ_CST);

		const size_t timeout_ms = SystemTimer::get().ms_since_boot() + 500;
		while (__atomic_load_n(&g_ap_running_count[0], __ATOMIC_SEQ_CST) < initialized_aps)
		{
			if (SystemTimer::get().ms_since_boot() >= timeout_ms)
				Kernel::panic("Could not start all APs ({}/{} started)", g_ap_running_count[0], initialized_aps);
			__builtin_ia32_pause();
		}

		dprintln("{} processors started", *g_ap_running_count);
	}


	void APIC::send_ipi(ProcessorID target)
	{
		ASSERT(Kernel::Processor::get_interrupt_state() == InterruptState::Disabled);
		while ((read_from_local_apic(LAPIC_ICR_LO_REG) & ICR_LO_delivery_status_send_pending) == ICR_LO_delivery_status_send_pending)
			__builtin_ia32_pause();
		write_to_local_apic(LAPIC_ICR_HI_REG, (read_from_local_apic(LAPIC_ICR_HI_REG) & 0x00FFFFFF) | (target.as_u32() << 24));
		write_to_local_apic(LAPIC_ICR_LO_REG,
			(read_from_local_apic(LAPIC_ICR_LO_REG) & ICR_LO_reserved_mask)
			| ICR_LO_delivery_mode_fixed
			| ICR_LO_destination_mode_physical
			| ICR_LO_level_assert
			| ICR_LO_trigger_mode_level
			| ICR_LO_destination_shorthand_none
			| IRQ_IPI
		);
	}

	void APIC::broadcast_ipi()
	{
		ASSERT(Kernel::Processor::get_interrupt_state() == InterruptState::Disabled);
		while ((read_from_local_apic(LAPIC_ICR_LO_REG) & ICR_LO_delivery_status_send_pending) == ICR_LO_delivery_status_send_pending)
			__builtin_ia32_pause();
		write_to_local_apic(LAPIC_ICR_HI_REG, (read_from_local_apic(LAPIC_ICR_HI_REG) & 0x00FFFFFF) | 0xFF000000);
		write_to_local_apic(LAPIC_ICR_LO_REG,
			(read_from_local_apic(LAPIC_ICR_LO_REG) & ICR_LO_reserved_mask)
			| ICR_LO_delivery_mode_fixed
			| ICR_LO_destination_mode_physical
			| ICR_LO_level_assert
			| ICR_LO_trigger_mode_level
			| ICR_LO_destination_shorthand_all_excluding_self
			| IRQ_IPI
		);
	}

	void APIC::enable()
	{
		write_to_local_apic(LAPIC_SIV_REG, read_from_local_apic(LAPIC_SIV_REG) | 0x1FF);
		initialize_timer();
	}

	static SpinLock s_timer_init_lock;

	void APIC::initialize_timer()
	{
		ASSERT(Kernel::Processor::get_interrupt_state() == InterruptState::Disabled);

		constexpr uint64_t measuring_duration_ms = 100;

		const bool needs_lock = SystemTimer::get().pre_scheduler_sleep_needs_lock();
		if (needs_lock)
			s_timer_init_lock.lock();

		write_to_local_apic(LAPIC_TIMER_LVT,         TimerModeOneShot | TimerMask);
		write_to_local_apic(LAPIC_TIMER_DIVIDE_REG,  DivideBy2);
		write_to_local_apic(LAPIC_TIMER_INITIAL_REG, 0xFFFFFFFF);
		SystemTimer::get().pre_scheduler_sleep_ns(measuring_duration_ms * 1'000'000);

		const uint32_t counter = read_from_local_apic(LAPIC_TIMER_CURRENT_REG);
		m_lapic_timer_frequency_hz = static_cast<uint64_t>(0xFFFFFFFF - counter) * 2 * (1000 / measuring_duration_ms);

		if (needs_lock)
			s_timer_init_lock.unlock(InterruptState::Disabled);

		dprintln("CPU {}: lapic timer frequency: {} Hz", Kernel::Processor::current_id(), m_lapic_timer_frequency_hz);

		write_to_local_apic(LAPIC_TIMER_LVT,         TimerModePeriodic | IRQ_TIMER);
		write_to_local_apic(LAPIC_TIMER_INITIAL_REG, m_lapic_timer_frequency_hz / 2 / 100);
	}

	uint32_t APIC::read_from_local_apic(ptrdiff_t offset)
	{
		return MMIO::read32(m_local_apic_vaddr + offset);
	}

	void APIC::write_to_local_apic(ptrdiff_t offset, uint32_t data)
	{
		MMIO::write32(m_local_apic_vaddr + offset, data);
	}

	uint32_t APIC::IOAPIC::read(uint8_t offset)
	{
		MMIO::write32(vaddr, offset);
		return MMIO::read32(vaddr + 16);
	}

	void APIC::IOAPIC::write(uint8_t offset, uint32_t data)
	{
		MMIO::write32(vaddr, offset);
		MMIO::write32(vaddr + 16, data);
	}

	void APIC::eoi(uint8_t)
	{
		write_to_local_apic(LAPIC_EIO_REG, 0);
	}

	void APIC::enable_irq(uint8_t irq)
	{
		SpinLockGuard _(m_lock);

		const uint32_t gsi = m_irq_overrides[irq];

		{
			int byte = gsi / 8;
			int bit  = gsi % 8;
			ASSERT(m_reserved_gsis[byte] & (1 << bit));
		}

		IOAPIC* ioapic = nullptr;
		for (IOAPIC& io : m_io_apics)
		{
			if (io.gsi_base <= gsi && gsi < io.gsi_base + io.max_redirs)
			{
				ioapic = &io;
				break;
			}
		}
		ASSERT(ioapic);

		const uint32_t pin = gsi - ioapic->gsi_base;

		RedirectionEntry redir;
		redir.lo_dword = ioapic->read(IOAPIC_REDIRS + pin * 2);
		redir.hi_dword = ioapic->read(IOAPIC_REDIRS + pin * 2 + 1);
		ASSERT(redir.mask); // TODO: handle overlapping interrupts

		redir.vector = IRQ_VECTOR_BASE + irq;
		redir.mask = 0;
		// FIXME: distribute IRQs more evenly?
		redir.destination = Kernel::Processor::bsp_id().as_u32();

		ioapic->write(IOAPIC_REDIRS + pin * 2,		redir.lo_dword);
		ioapic->write(IOAPIC_REDIRS + pin * 2 + 1,	redir.hi_dword);
	}

	bool APIC::is_in_service(uint8_t irq)
	{
		uint32_t dword = (irq + IRQ_VECTOR_BASE) / 32;
		uint32_t bit = (irq + IRQ_VECTOR_BASE) % 32;

		uint32_t isr = read_from_local_apic(LAPIC_IS_REG + dword * 0x10);
		return isr & (1 << bit);
	}

	BAN::ErrorOr<void> APIC::reserve_irq(uint8_t irq)
	{
		SpinLockGuard _(m_lock);

		const uint32_t gsi = m_irq_overrides[irq];

		bool found_ioapic = false;
		for (const auto& io : m_io_apics)
		{
			if (io.gsi_base <= gsi && gsi < io.gsi_base + io.max_redirs)
			{
				found_ioapic = true;
				break;
			}
		}

		if (!found_ioapic)
		{
			dwarnln("No IOAPIC for GSI {}", gsi);
			return BAN::Error::from_errno(EINVAL);
		}

		int byte = gsi / 8;
		int bit  = gsi % 8;
		if (m_reserved_gsis[byte] & (1 << bit))
		{
			dwarnln("GSI {} is already reserved (IRQ {})", gsi, irq);
			return BAN::Error::from_errno(EFAULT);
		}
		m_reserved_gsis[byte] |= 1 << bit;
		return {};
	}

	// FIXME: rewrite gsi and vector reserving
	//        this is a hack to allow direct GSI reservation
	BAN::ErrorOr<uint8_t> APIC::reserve_gsi(uint32_t gsi)
	{
		size_t irq = 0;
		for (; irq < 0x100; irq++)
			if (m_irq_overrides[irq] == gsi)
				break;

		if (irq == 0x100)
		{
			dwarnln("TODO: reserve GSI not accessible through overrides");
			return BAN::Error::from_errno(ENOTSUP);
		}

		TRY(reserve_irq(irq));

		return irq;
	}

	BAN::Optional<uint8_t> APIC::get_free_irq()
	{
		SpinLockGuard _(m_lock);
		for (uint8_t irq = 0; irq < m_irq_count; irq++)
		{
			const uint8_t gsi = m_irq_overrides[irq];

			IOAPIC* ioapic = nullptr;
			for (IOAPIC& io : m_io_apics)
			{
				if (io.gsi_base <= gsi && gsi < io.gsi_base + io.max_redirs)
				{
					ioapic = &io;
					break;
				}
			}

			if (!ioapic)
				continue;

			const uint8_t byte = gsi / 8;
			const uint8_t bit  = gsi % 8;
			if (m_reserved_gsis[byte] & (1 << bit))
				continue;
			m_reserved_gsis[byte] |= 1 << bit;
			return irq;
		}
		return {};
	}

}
