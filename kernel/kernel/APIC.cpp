#include <BAN/ScopeGuard.h>
#include <kernel/Debug.h>
#include <kernel/APIC.h>
#include <kernel/CPUID.h>
#include <kernel/IDT.h>
#include <kernel/MMU.h>

#include <string.h>

#define LAPIC_EIO_REG		0xB0
#define LAPIC_SIV_REG		0xF0
#define LAPIC_IS_REG		0x100

#define IOAPIC_MAX_REDIRS	0x01
#define IOAPIC_REDIRS		0x10

#define DEBUG_PRINT_PROCESSORS 0

// https://uefi.org/specs/ACPI/6.5/05_ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt-format

static constexpr uint32_t RSPD_SIZE		= 20;
static constexpr uint32_t RSPDv2_SIZE	= 36;

struct RSDP
{
	char		signature[8];
	uint8_t		checksum;
	char		OEMID[6];
	uint8_t		revision;
	uint32_t	rsdt_address;
	uint32_t	v2_length;
	uint64_t	v2_xsdt_address;
	uint8_t		v2_extended_checksum;
	uint8_t		v2_reserved[3];
} __attribute__ ((packed));

struct SDTHeader
{
	char		signature[4];
	uint32_t	length;
	uint8_t		revision;
	uint8_t		checksum;
	char		OEMID[6];
	char		OEM_table_id[8];
	uint32_t	OEM_revision;
	uint32_t	creator_id;
	uint32_t	creator_revision;
} __attribute__((packed));

struct MADT
{
	SDTHeader header;
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

static bool is_rsdp(uintptr_t rsdp_addr)
{
	const RSDP* rsdp = (const RSDP*)rsdp_addr;

	if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0)
		return false;

	{
		uint8_t checksum = 0;
		for (uint32_t i = 0; i < RSPD_SIZE; i++)
			checksum += ((uint8_t*)rsdp)[i];
		if (checksum != 0)
			return false;
	}

	if (rsdp->revision == 2)
	{
		uint8_t checksum = 0;
		for (uint32_t i = 0; i < RSPDv2_SIZE; i++)
			checksum += ((uint8_t*)rsdp)[i];
		if (checksum != 0)
			return false;
	}

	return true;
}

static uintptr_t locate_rsdp()
{
	// Look in main BIOS area below 1 MB
	for (uintptr_t addr = 0x000E0000; addr < 0x000FFFFF; addr += 16)
		if (is_rsdp(addr))
			return addr;
	return 0;
}

static bool is_valid_std_header(const SDTHeader* header)
{
	uint8_t sum = 0;
	for (uint32_t i = 0; i < header->length; i++)
		sum += ((uint8_t*)header)[i];
	return sum == 0;
}

uintptr_t locate_madt(uintptr_t rsdp_addr)
{
	uintptr_t entry_address_base	= 0;
	ptrdiff_t entry_pointer_size	= 0;
	uint32_t entry_count			= 0;

	const RSDP* rsdp = (const RSDP*)rsdp_addr;
	if (rsdp->revision == 2)
	{
		uintptr_t xsdt_addr = rsdp->v2_xsdt_address;
		MMU::get().allocate_page(xsdt_addr, MMU::Flags::ReadWrite | MMU::Flags::Present);
		entry_address_base = xsdt_addr + sizeof(SDTHeader);
		entry_count = (((const SDTHeader*)xsdt_addr)->length - sizeof(SDTHeader)) / 8;
		entry_pointer_size = 8;
		MMU::get().unallocate_page(xsdt_addr);
	}
	else
	{
		uintptr_t rsdt_addr = rsdp->rsdt_address;
		MMU::get().allocate_page(rsdt_addr, MMU::Flags::ReadWrite | MMU::Flags::Present);
		entry_address_base = rsdt_addr + sizeof(SDTHeader);
		entry_count = (((const SDTHeader*)rsdt_addr)->length - sizeof(SDTHeader)) / 4;
		entry_pointer_size = 4;
		MMU::get().unallocate_page(rsdt_addr);
	}

	for (uint32_t i = 0; i < entry_count; i++)
	{
		uintptr_t entry_addr_ptr = entry_address_base + i * entry_pointer_size;
		MMU::get().allocate_page(entry_addr_ptr, MMU::Flags::ReadWrite | MMU::Flags::Present);

		union dummy { uint32_t addr32; uint64_t addr64; } __attribute__((aligned(1), packed));

		uintptr_t entry_addr;		
		if (entry_pointer_size == 4)
			entry_addr = ((dummy*)entry_addr_ptr)->addr32;
		else
			entry_addr = ((dummy*)entry_addr_ptr)->addr64;

		MMU::get().allocate_page(entry_addr, MMU::Flags::ReadWrite | MMU::Flags::Present);

		BAN::ScopeGuard _([&]() {
			MMU::get().unallocate_page(entry_addr);
			MMU::get().unallocate_page(entry_addr_ptr);
		});

		const SDTHeader* entry = (const SDTHeader*)entry_addr;
		if (memcmp(entry->signature, "APIC", 4) == 0 && is_valid_std_header(entry))
			return entry_addr;
	}

	return 0;
}

APIC* APIC::create()
{
	uint32_t ecx, edx;
	CPUID::get_features(ecx, edx);
	if (!(edx & CPUID::Features::EDX_APIC))
	{
		dprintln("Local APIC is not available");
		return nullptr;
	}

	uintptr_t rsdp_addr = locate_rsdp();
	if (!rsdp_addr)
	{
		dprintln("Could not locate RSDP");
		return nullptr;
	}

	uintptr_t madt_addr = locate_madt(rsdp_addr);
	if (!madt_addr)
	{
		dprintln("Could not find MADT in RSDP");
		return nullptr;
	}

	MMU::get().allocate_page(madt_addr, MMU::Flags::ReadWrite | MMU::Flags::Present);

	const MADT* madt = (const MADT*)madt_addr;

	APIC* apic = new APIC;
	apic->m_local_apic = madt->local_apic;
	for (uint32_t i = 0x00; i <= 0xFF; i++)
		apic->m_irq_overrides[i] = i;

	uintptr_t madt_entry_addr = (uintptr_t)madt + sizeof(MADT);
	while (madt_entry_addr < (uintptr_t)madt + madt->header.length)
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
				ioapic.address		= entry->entry1.ioapic_address;
				ioapic.gsi_base		= entry->entry1.gsi_base;
				ioapic.max_redirs	= 0;
				MUST(apic->m_io_apics.push_back(ioapic));
				break;
			case 2:
				apic->m_irq_overrides[entry->entry2.irq_source] = entry->entry2.gsi;
				break;
			case 5:
				apic->m_local_apic = entry->entry5.address;
				break;
			default:
				dprintln("Unhandled madt entry, type {}", entry->type);
				break;
		}
		madt_entry_addr += entry->length;
	}
	MMU::get().unallocate_page((uintptr_t)madt);

	if (apic->m_local_apic == 0 || apic->m_io_apics.empty())
	{
		dprintln("MADT did not provide necessary information");
		delete apic;
		return nullptr;
	}

	MMU::get().allocate_page(apic->m_local_apic, MMU::Flags::ReadWrite | MMU::Flags::Present);
	for (auto& io_apic : apic->m_io_apics)
	{
		MMU::get().allocate_page(io_apic.address, MMU::Flags::ReadWrite | MMU::Flags::Present);
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
	return *(uint32_t*)(m_local_apic + offset);
}

void APIC::write_to_local_apic(ptrdiff_t offset, uint32_t data)
{
	*(uint32_t*)(m_local_apic + offset) = data;
}

uint32_t APIC::IOAPIC::read(uint8_t offset)
{
	volatile uint32_t* ioapic = (volatile uint32_t*)address;
	ioapic[0] = offset;
	return ioapic[4];
}

void APIC::IOAPIC::write(uint8_t offset, uint32_t data)
{
	volatile uint32_t* ioapic = (volatile uint32_t*)address;
	ioapic[0] = offset;
	ioapic[4] = data;
}

void APIC::eoi(uint8_t)
{
	write_to_local_apic(LAPIC_EIO_REG, 0);	
}

void APIC::enable_irq(uint8_t irq)
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
	ASSERT(ioapic);

	RedirectionEntry redir;
	redir.lo_dword = ioapic->read(IOAPIC_REDIRS + gsi * 2);
	redir.hi_dword = ioapic->read(IOAPIC_REDIRS + gsi * 2 + 1);

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