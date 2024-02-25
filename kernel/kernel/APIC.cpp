#include <BAN/ScopeGuard.h>
#include <kernel/ACPI.h>
#include <kernel/APIC.h>
#include <kernel/CPUID.h>
#include <kernel/Debug.h>
#include <kernel/IDT.h>
#include <kernel/Lock/LockGuard.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/MMIO.h>

#include <string.h>

#define LAPIC_EIO_REG		0xB0
#define LAPIC_SIV_REG		0xF0
#define LAPIC_IS_REG		0x100

#define IOAPIC_MAX_REDIRS	0x01
#define IOAPIC_REDIRS		0x10

#define DEBUG_PRINT_PROCESSORS 0

// https://uefi.org/specs/ACPI/6.5/05_ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt-format

namespace Kernel
{

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

		const MADT* madt = (const MADT*)Kernel::ACPI::get().get_header("APIC"sv, 0);
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
					Processor processor;
					processor.processor_id	= entry->entry0.acpi_processor_id;
					processor.apic_id		= entry->entry0.apic_id;
					processor.flags			= entry->entry0.flags & 0x03;
					MUST(apic->m_processors.push_back(processor));
					break;
				case 1:
					IOAPIC ioapic;
					ioapic.id			= entry->entry1.ioapic_id;
					ioapic.paddr		= entry->entry1.ioapic_address;
					ioapic.gsi_base		= entry->entry1.gsi_base;
					ioapic.max_redirs	= 0;
					MUST(apic->m_io_apics.push_back(ioapic));
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

		// Mask all interrupts
		uint32_t sivr = apic->read_from_local_apic(LAPIC_SIV_REG);
		apic->write_to_local_apic(LAPIC_SIV_REG, sivr | 0x1FF);

	#if DEBUG_PRINT_PROCESSORS
		for (auto& processor : apic->m_processors)
		{
			dprintln("Processor{}", processor.processor_id);
			dprintln("  lapic id: {}", processor.apic_id);
			dprintln("  status:   {}", (processor.flags & Processor::Flags::Enabled) ? "enabled" : (processor.flags & Processor::Flags::OnlineCapable) ? "can be enabled" : "disabled");
		}
	#endif

		return apic;
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
		LockGuard _(m_lock);

		uint32_t gsi = m_irq_overrides[irq];

		{
			int byte = gsi / 8;
			int bit  = gsi % 8;
			ASSERT(m_reserved_gsis[byte] & (1 << bit));
		}

		IOAPIC* ioapic = nullptr;
		for (IOAPIC& io : m_io_apics)
		{
			if (io.gsi_base <= gsi && gsi <= io.gsi_base + io.max_redirs)
			{
				ioapic = &io;
				break;
			}
		}
		ASSERT(ioapic);

		RedirectionEntry redir;
		redir.lo_dword = ioapic->read(IOAPIC_REDIRS + gsi * 2);
		redir.hi_dword = ioapic->read(IOAPIC_REDIRS + gsi * 2 + 1);
		ASSERT(redir.mask); // TODO: handle overlapping interrupts

		redir.vector = IRQ_VECTOR_BASE + irq;
		redir.mask = 0;
		redir.destination = m_processors.front().apic_id;

		ioapic->write(IOAPIC_REDIRS + gsi * 2,		redir.lo_dword);
		ioapic->write(IOAPIC_REDIRS + gsi * 2 + 1,	redir.hi_dword);
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
		LockGuard _(m_lock);

		uint32_t gsi = m_irq_overrides[irq];

		IOAPIC* ioapic = nullptr;
		for (IOAPIC& io : m_io_apics)
		{
			if (io.gsi_base <= gsi && gsi <= io.gsi_base + io.max_redirs)
			{
				ioapic = &io;
				break;
			}
		}

		if (!ioapic)
		{
			dwarnln("Cannot enable irq {} for APIC", irq);
			return BAN::Error::from_errno(EFAULT);
		}

		int byte = gsi / 8;
		int bit  = gsi % 8;
		if (m_reserved_gsis[byte] & (1 << bit))
		{
			dwarnln("irq {} is already reserved", irq);
			return BAN::Error::from_errno(EFAULT);
		}
		m_reserved_gsis[byte] |= 1 << bit;
		return {};
	}

	BAN::Optional<uint8_t> APIC::get_free_irq()
	{
		LockGuard _(m_lock);
		for (int irq = 0; irq <= 0xFF; irq++)
		{
			uint32_t gsi = m_irq_overrides[irq];

			IOAPIC* ioapic = nullptr;
			for (IOAPIC& io : m_io_apics)
			{
				if (io.gsi_base <= gsi && gsi <= io.gsi_base + io.max_redirs)
				{
					ioapic = &io;
					break;
				}
			}

			if (!ioapic)
				continue;

			int byte = gsi / 8;
			int bit  = gsi % 8;
			if (m_reserved_gsis[byte] & (1 << bit))
				continue;
			m_reserved_gsis[byte] |= 1 << bit;
			return irq;
		}
		return {};
	}

}
