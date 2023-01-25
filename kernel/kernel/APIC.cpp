#include <BAN/ScopeGuard.h>
#include <kernel/APIC.h>
#include <kernel/CPUID.h>
#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/MMU.h>
#include <kernel/Serial.h>

#include <string.h>

#define LAPIC_EIO_REG		0xB0
#define LAPIC_SIV_REG		0xF0
#define LAPIC_IS_REG		0x100

#define IOAPIC_MAX_REDIRS	0x01
#define IOAPIC_REDIRS		0x10

// https://uefi.org/specs/ACPI/6.5/05_ACPI_Software_Programming_Model.html#multiple-apic-description-table-madt-format

struct RSDPDescriptor
{
	char signature[8];
	uint8_t checksum;
	char OEMID[6];
	uint8_t revision;
	uint32_t rsdt_address;
} __attribute__ ((packed));

struct RSDPDescriptor20
{
	RSDPDescriptor first_part;
	uint32_t length;
	uint64_t xsdt_address;
	uint8_t extended_checksum;
	uint8_t reserved[3];
} __attribute__((packed));

struct ACPISDTHeader
{
	char signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	char OEMID[6];
	char OEM_table_id[8];
	uint32_t OEM_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
} __attribute__((packed));

struct RSDT
{
	ACPISDTHeader header;
	uint32_t sdt_pointer[0];
};

struct XSDT
{
	ACPISDTHeader header;
	uint64_t sdt_pointer[0];
};

struct MADT
{
	ACPISDTHeader header;
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

static bool IsRSDP(const RSDPDescriptor* rsdp)
{
	if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0)
		return false;

	{
		uint8_t checksum = 0;
		for (uint32_t i = 0; i < sizeof(RSDPDescriptor); i++)
			checksum += ((uint8_t*)rsdp)[i];
		if (checksum != 0)
			return false;
	}

	if (rsdp->revision == 2)
	{
		RSDPDescriptor20* rsdp20 = (RSDPDescriptor20*)rsdp;
		uint8_t checksum = 0;
		for (uint32_t i = 0; i < sizeof(RSDPDescriptor20); i++)
			checksum += ((uint8_t*)rsdp20)[i];
		if (checksum != 0)
			return false;
	}

	return true;
}

static const RSDPDescriptor* LocateRSDP()
{
	// Look in main BIOS area below 1 MB
	for (uintptr_t addr = 0x000E0000; addr < 0x000FFFFF; addr += 16)
		if (IsRSDP((RSDPDescriptor*)addr))
			return (RSDPDescriptor*)addr;
	return nullptr;
}

static bool IsValidACPISDTHeader(const ACPISDTHeader* header)
{
	uint8_t sum = 0;
	for (uint32_t i = 0; i < header->length; i++)
		sum += ((uint8_t*)header)[i];
	return sum == 0;
}

static const MADT* LocateMADT(const RSDPDescriptor* rsdp)
{
	uintptr_t root_addr = 0;
	uint32_t entry_count = 0;
	if (rsdp->revision == 2)
	{
		const XSDT* root = (const XSDT*)((const RSDPDescriptor20*)rsdp)->xsdt_address;
		MMU::Get().AllocatePage((uintptr_t)root);
		entry_count = (root->header.length - sizeof(root->header)) / sizeof(*root->sdt_pointer);
		root_addr = (uintptr_t)root;
	}
	else
	{
		const RSDT* root = (const RSDT*)(uintptr_t)rsdp->rsdt_address;
		MMU::Get().AllocatePage((uintptr_t)root);
		entry_count = (root->header.length - sizeof(root->header)) / sizeof(*root->sdt_pointer);
		root_addr = (uintptr_t)root;
	}

	BAN::ScopeGuard guard([root_addr]() { MMU::Get().UnAllocatePage(root_addr); });

	for (uint32_t i = 0; i < entry_count; i++)
	{
		const ACPISDTHeader* header = nullptr;
		if (rsdp->revision == 2)
			header = (const ACPISDTHeader*)((const XSDT*)root_addr)->sdt_pointer[i];
		else
			header = (const ACPISDTHeader*)(uintptr_t)((const RSDT*)root_addr)->sdt_pointer[i];
		if (memcmp(header->signature, "APIC", 4) == 0 && IsValidACPISDTHeader(header))
			return (const MADT*)header;
	}

	return nullptr;
}

APIC* APIC::Create()
{
	uint32_t ecx, edx;
	CPUID::GetFeatures(ecx, edx);
	if (!(edx & CPUID::Features::EDX_APIC))
	{
		dprintln("Local APIC is not available");
		return nullptr;
	}

	const RSDPDescriptor* rsdp = LocateRSDP();
	if (rsdp == nullptr)
	{
		dprintln("Could not locate RSDP");
		return nullptr;
	}

	const MADT* madt = LocateMADT(rsdp);
	if (madt == nullptr)
	{
		dprintln("Could not find MADT in RSDP");
		return nullptr;
	}

	MMU::Get().AllocatePage((uintptr_t)madt);

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
				MUST(apic->m_processors.PushBack(processor));
				break;
			case 1:
				IOAPIC ioapic;
				ioapic.id			= entry->entry1.ioapic_id;
				ioapic.address		= entry->entry1.ioapic_address;
				ioapic.gsi_base		= entry->entry1.gsi_base;
				ioapic.max_redirs	= 0;
				MUST(apic->m_io_apics.PushBack(ioapic));
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
	MMU::Get().UnAllocatePage((uintptr_t)madt);

	if (apic->m_local_apic == 0 || apic->m_io_apics.Empty())
	{
		dprintln("MADT did not provide necessary information");
		delete apic;
		return nullptr;
	}

	MMU::Get().AllocatePage(apic->m_local_apic);
	for (auto& io_apic : apic->m_io_apics)
	{
		MMU::Get().AllocatePage(io_apic.address);
		io_apic.max_redirs = io_apic.Read(IOAPIC_MAX_REDIRS);
	}

	// Mask all interrupts
	uint32_t sivr = apic->ReadFromLocalAPIC(LAPIC_SIV_REG);
	apic->WriteToLocalAPIC(LAPIC_SIV_REG, sivr | 0x1FF);

	return apic;
}

uint32_t APIC::ReadFromLocalAPIC(ptrdiff_t offset)
{
	return *(uint32_t*)(m_local_apic + offset);
}

void APIC::WriteToLocalAPIC(ptrdiff_t offset, uint32_t data)
{
	*(uint32_t*)(m_local_apic + offset) = data;
}

uint32_t APIC::IOAPIC::Read(uint8_t offset)
{
	volatile uint32_t* ioapic = (volatile uint32_t*)address;
	ioapic[0] = offset;
	return ioapic[4];
}

void APIC::IOAPIC::Write(uint8_t offset, uint32_t data)
{
	volatile uint32_t* ioapic = (volatile uint32_t*)address;
	ioapic[0] = offset;
	ioapic[4] = data;
}

void APIC::EOI(uint8_t)
{
	WriteToLocalAPIC(LAPIC_EIO_REG, 0);	
}

void APIC::EnableIrq(uint8_t irq)
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
	redir.lo_dword = ioapic->Read(IOAPIC_REDIRS + gsi * 2);
	redir.hi_dword = ioapic->Read(IOAPIC_REDIRS + gsi * 2 + 1);

	redir.vector = IRQ_VECTOR_BASE + irq;
	redir.mask = 0;
	redir.destination = m_processors.Front().apic_id;

	ioapic->Write(IOAPIC_REDIRS + gsi * 2,		redir.lo_dword);
	ioapic->Write(IOAPIC_REDIRS + gsi * 2 + 1,	redir.hi_dword);
}

void APIC::GetISR(uint32_t out[8])
{
	for (uint32_t i = 0; i < 8; i++)
		out[i] = ReadFromLocalAPIC(LAPIC_IS_REG + i * 0x10);
}