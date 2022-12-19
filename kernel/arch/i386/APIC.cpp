#include <kernel/APIC.h>
#include <kernel/CPUID.h>
#include <kernel/IDT.h>
#include <kernel/IO.h>
#include <kernel/kprint.h>
#include <kernel/panic.h>
#include <kernel/PIC.h>
#include <kernel/Serial.h>

#include <stdint.h>
#include <string.h>

namespace APIC
{

	static bool s_using_fallback_pic = false;

	static uint32_t s_local_apic	= 0;
	static uint32_t s_io_apic		= 0;

	static uint8_t s_overrides[0x100];

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
		RSDPDescriptor firstPart;
		uint32_t length;
		uint64_t xsdt_address;
		uint8_t extended_checksum;
		uint8_t reserved[3];
	} __attribute__((packed));

	struct ACPISDTHeader
	{
		char signature[4];
		uint32_t length;
		uint8_t eevision;
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
				uint8_t processor_id;
				uint8_t apic_id;
				uint32_t flags;
			} __attribute__((packed)) lapic;
			struct
			{
				uint8_t ioapic_id;
				uint8_t reserved;
				uint32_t ioapic_address;
				uint32_t gsi_base;
			} __attribute__((packed)) ioapic;
			struct
			{
				uint8_t bus_source;
				uint8_t irq_source;
				uint32_t gsi;
				uint16_t flags;
			} __attribute__((packed)) interrupt_source_override;	
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
	

	static uint32_t ReadLocalAPIC(uint32_t offset);
	static void WriteLocalAPIC(uint32_t offset, uint32_t value);
	static uint32_t ReadIOAPIC(uint8_t reg);
	static void WriteIOAPIC(uint8_t reg, uint32_t value);


	static bool IsRSDP(RSDPDescriptor* rsdp)
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

	static RSDPDescriptor* LocateRSDP()
	{
		// Look in main BIOS area below 1 MB
		for (uint32_t addr = 0x000E0000; addr < 0x000FFFFF; addr += 16)
			if (IsRSDP((RSDPDescriptor*)addr))
				return (RSDPDescriptor*)addr;

		return nullptr;
	}

	static bool IsValidACPISDTHeader(ACPISDTHeader* header)
	{
		uint8_t sum = 0;
		for (uint32_t i = 0; i < header->length; i++)
			sum += ((uint8_t*)header)[i];
		return sum == 0;
	}

	template<typename SDT>
	static void GetAPIC(SDT* root)
	{
		uint32_t sdt_entry_count = (root->header.length - sizeof(root->header)) / sizeof(*(root->sdt_pointer));
		for (uint32_t i = 0; i < sdt_entry_count; i++)
		{
			ACPISDTHeader* header = (ACPISDTHeader*)root->sdt_pointer[i];
			if (!IsValidACPISDTHeader(header))
				continue;
			if (memcmp(header->signature, "APIC", 4) == 0)
			{
				MADT* madt = (MADT*)header;
				s_local_apic = madt->local_apic;
				uint32_t entry_addr = (uint32_t)madt + sizeof(MADT);
				while (entry_addr < (uint32_t)madt + madt->header.length)
				{
					MADTEntry* entry = (MADTEntry*)entry_addr;

					switch (entry->type)
					{
						case 0:
							//s_processor_id = entry->lapic.processor_id;
							kprintln("Processor {} ({})", entry->lapic.processor_id, entry->lapic.apic_id);
							break;
						case 1:
							if (s_io_apic == 0)
								s_io_apic = entry->ioapic.ioapic_address;
							kprintln("IOAPIC #{}", entry->ioapic.ioapic_id);
							kprintln("  addr:     {}", (void*)entry->ioapic.ioapic_address);
							kprintln("  gsi base: {} ({})", entry->ioapic.gsi_base, (uint8_t)((ReadIOAPIC(0x01) >> 16) + 1));
							break;
						case 2:
							s_overrides[entry->interrupt_source_override.irq_source] = entry->interrupt_source_override.gsi;
							break;
						default:
							break;
					}
					entry_addr += entry->length;
				}
			}
		}
	}


	static uint32_t ReadLocalAPIC(uint32_t offset)
	{
		return *(uint32_t*)(s_local_apic + offset);
	}

	static void WriteLocalAPIC(uint32_t offset, uint32_t value)
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
		*(uint32_t*)(s_local_apic + offset) = value;
#pragma GCC diagnostic pop
	}

	static uint32_t ReadIOAPIC(uint8_t reg)
	{
		volatile uint32_t* ioapic = (volatile uint32_t*)s_io_apic;
		ioapic[0] = reg;
		return ioapic[4];
	}
	
	static void WriteIOAPIC(uint8_t reg, uint32_t value)
	{
		volatile uint32_t* ioapic = (volatile uint32_t*)s_io_apic;
		ioapic[0] = reg;
		ioapic[4] = value;
	}

	static bool InitializeAPIC()
	{
		if (!CPUID::IsAvailable())
		{
			kprintln("CPUID not available");
			return false;
		}

		uint32_t ecx, edx;
		CPUID::GetFeatures(ecx, edx);
		if (!(edx & CPUID::Features::EDX_APIC))
		{
			kprintln("Local APIC not available");
			return false;
		}
		if (!(edx & CPUID::Features::EDX_MSR))
		{
			kprintln("MSR not available");
			return false;
		}

		RSDPDescriptor* rsdp = LocateRSDP();
		if (rsdp == nullptr)
		{
			kprintln("Could not locate RSDP");
			return false;
		}

		if (rsdp->revision == 2)
		{
			GetAPIC<XSDT>((XSDT*)((RSDPDescriptor20*)rsdp)->xsdt_address);
		}
		else
		{
			GetAPIC<RSDT>((RSDT*)rsdp->rsdt_address);
		}

		if (s_local_apic == 0 || s_io_apic == 0)
			return false;

		// Enable Local APIC
		uint32_t sipi = ReadLocalAPIC(0xF0);
		WriteIOAPIC(0xF0, sipi | 0x1FF);

		return true;
	}

	void Initialize()
	{
		for (uint32_t i = 0x00; i <= 0xFF; i++)
			s_overrides[i] = i;

		PIC::MaskAll();
		PIC::Remap();

		if (!InitializeAPIC())
		{
			kprintln("Could not initialize APIC. Using PIC as fallback");
			s_using_fallback_pic = true;
		}
	}

	void EOI()
	{
		if (s_using_fallback_pic)
			return PIC::EOI(0);
		WriteLocalAPIC(0xB0, 0);
	}

	void GetISR(uint32_t out[8])
	{
		if (s_using_fallback_pic)
		{
			memset(out, 0, sizeof(uint32_t) * 8);
			uint32_t addr = (uint32_t)out + IRQ_VECTOR_BASE / 8;
			*(uint16_t*)addr = PIC::GetISR();
			return;
		}

		for (uint32_t i = 0; i < 8; i++)
			out[i] = ReadLocalAPIC(0x100 + i * 0x10);
	}

	void EnableIRQ(uint8_t irq)
	{
		if (s_using_fallback_pic)
			return PIC::Unmask(irq);

		uint32_t gsi = s_overrides[irq];
		
		RedirectionEntry redir;
		redir.lo_dword = ReadIOAPIC(0x10 + gsi * 2);
		redir.hi_dword = ReadIOAPIC(0x10 + gsi * 2 + 1);

		redir.vector = IRQ_VECTOR_BASE + irq;
		redir.mask = 0;
		redir.destination = ReadLocalAPIC(0x20);

		kprintln("register irq {2} -> {2} @ apic {}", irq, gsi, redir.destination);

		WriteIOAPIC(0x10 + gsi * 2,     redir.lo_dword);
		WriteIOAPIC(0x10 + gsi * 2 + 1, redir.hi_dword);
	}

}