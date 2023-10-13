#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/ACPI.h>
#include <kernel/Memory/PageTable.h>

#include <lai/core.h>

#define RSPD_SIZE	20
#define RSPDv2_SIZE	36

namespace Kernel
{

	struct RSDP
	{
		uint8_t signature[8];
		uint8_t checksum;
		uint8_t oemid[6];
		uint8_t revision;
		uint32_t rsdt_address;

		// only in revision >= 2
		uint32_t length;
		uint64_t xsdt_address;
		uint8_t extended_checksum;
		uint8_t reserved[3];
	};

	struct RSDT : public ACPI::SDTHeader
	{
		uint32_t entries[];
	} __attribute__((packed));

	struct XSDT : public ACPI::SDTHeader
	{
		uint64_t entries[];
	} __attribute__((packed));

	static ACPI* s_instance = nullptr;

	BAN::ErrorOr<void> ACPI::initialize()
	{
		ASSERT(s_instance == nullptr);
		s_instance = new ACPI;
		if (s_instance == nullptr)
			return BAN::Error::from_errno(ENOMEM);
		TRY(s_instance->initialize_impl());
		lai_create_namespace();
		return {};
	}

	ACPI& ACPI::get()
	{
		ASSERT(s_instance != nullptr);
		return *s_instance;
	}

	static bool is_rsdp(uintptr_t rsdp_addr)
	{
		const RSDP* rsdp = (const RSDP*)rsdp_addr;

		if (memcmp(rsdp->signature, "RSD PTR ", 8) != 0)
			return false;

		{
			uint8_t checksum = 0;
			for (uint32_t i = 0; i < RSPD_SIZE; i++)
				checksum += ((const uint8_t*)rsdp)[i];
			if (checksum != 0)
				return false;
		}

		if (rsdp->revision == 2)
		{
			uint8_t checksum = 0;
			for (uint32_t i = 0; i < RSPDv2_SIZE; i++)
				checksum += ((const uint8_t*)rsdp)[i];
			if (checksum != 0)
				return false;
		}

		return true;
	}

	static const RSDP* locate_rsdp()
	{
		// Look in main BIOS area below 1 MB
		for (uintptr_t addr = P2V(0x000E0000); addr < P2V(0x000FFFFF); addr += 16)
			if (is_rsdp(addr))
				return (const RSDP*)addr;
		return nullptr;
	}

	static bool is_valid_std_header(const ACPI::SDTHeader* header)
	{
		uint8_t sum = 0;
		for (uint32_t i = 0; i < header->length; i++)
			sum += ((uint8_t*)header)[i];
		return sum == 0;
	}

	BAN::ErrorOr<void> ACPI::initialize_impl()
	{
		const RSDP* rsdp = locate_rsdp();
		if (rsdp == nullptr)
			return BAN::Error::from_error_code(ErrorCode::ACPI_NoRootSDT);
		lai_set_acpi_revision(rsdp->revision);

		uint32_t root_entry_count = 0;

		if (rsdp->revision >= 2)
		{
			PageTable::kernel().map_page_at(rsdp->xsdt_address & PAGE_ADDR_MASK, 0, PageTable::Flags::Present);
			const XSDT* xsdt = (const XSDT*)(rsdp->xsdt_address % PAGE_SIZE);
			BAN::ScopeGuard _([xsdt] { PageTable::kernel().unmap_page(0); });

			if (memcmp(xsdt->signature, "XSDT", 4) != 0)
				return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);
			if (!is_valid_std_header(xsdt))
				return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);

			m_header_table_paddr = (paddr_t)xsdt->entries + (rsdp->rsdt_address & PAGE_ADDR_MASK);
			m_entry_size = 8;
			root_entry_count = (xsdt->length - sizeof(SDTHeader)) / 8;
		}
		else
		{
			PageTable::kernel().map_page_at(rsdp->rsdt_address & PAGE_ADDR_MASK, 0, PageTable::Flags::Present);
			const RSDT* rsdt = (const RSDT*)((vaddr_t)rsdp->rsdt_address % PAGE_SIZE);
			BAN::ScopeGuard _([rsdt] { PageTable::kernel().unmap_page(0); });

			if (memcmp(rsdt->signature, "RSDT", 4) != 0)
				return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);
			if (!is_valid_std_header(rsdt))
				return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);

			m_header_table_paddr = (paddr_t)rsdt->entries + (rsdp->rsdt_address & PAGE_ADDR_MASK);
			m_entry_size = 4;
			root_entry_count = (rsdt->length - sizeof(SDTHeader)) / 4;
		}

		size_t needed_pages = range_page_count(m_header_table_paddr, root_entry_count * m_entry_size);
		m_header_table_vaddr = PageTable::kernel().reserve_free_contiguous_pages(needed_pages, KERNEL_OFFSET);
		ASSERT(m_header_table_vaddr);

		m_header_table_vaddr += m_header_table_paddr % PAGE_SIZE;

		PageTable::kernel().map_range_at(
			m_header_table_paddr & PAGE_ADDR_MASK,
			m_header_table_vaddr & PAGE_ADDR_MASK,
			needed_pages * PAGE_SIZE,
			PageTable::Flags::Present
		);

		auto map_header =
			[](paddr_t header_paddr) -> vaddr_t
			{
				PageTable::kernel().map_page_at(header_paddr & PAGE_ADDR_MASK, 0, PageTable::Flags::Present);
				size_t header_length = ((SDTHeader*)(header_paddr % PAGE_SIZE))->length;
				PageTable::kernel().unmap_page(0);

				size_t needed_pages = range_page_count(header_paddr, header_length);
				vaddr_t page_vaddr = PageTable::kernel().reserve_free_contiguous_pages(needed_pages, KERNEL_OFFSET);
				ASSERT(page_vaddr);
				
				PageTable::kernel().map_range_at(
					header_paddr & PAGE_ADDR_MASK,
					page_vaddr,
					needed_pages * PAGE_SIZE,
					PageTable::Flags::Present
				);

				auto* header = (SDTHeader*)(page_vaddr + (header_paddr % PAGE_SIZE));
				if (!is_valid_std_header(header))
				{
					PageTable::kernel().unmap_range(page_vaddr, needed_pages * PAGE_SIZE);
					return 0;
				}

				return page_vaddr + (header_paddr % PAGE_SIZE);
			};

		for (uint32_t i = 0; i < root_entry_count; i++)
		{
			paddr_t header_paddr = (m_entry_size == 4) ?
				((uint32_t*)m_header_table_vaddr)[i] :
				((uint64_t*)m_header_table_vaddr)[i];
			
			vaddr_t header_vaddr = map_header(header_paddr);
			if (header_vaddr == 0)
				continue;

			MUST(m_mapped_headers.push_back({
				.paddr = header_paddr,
				.vaddr = header_vaddr
			}));
		}

		for (size_t i = 0; i < m_mapped_headers.size(); i++)
		{
			auto* header = m_mapped_headers[i].as_header();
			dprintln("found header {}", *header);

			if (memcmp(header->signature, "FACP", 4) == 0)
			{
				auto* fadt = (FADT*)header;
				paddr_t dsdt_paddr = fadt->x_dsdt;
				if (dsdt_paddr == 0 || !PageTable::is_valid_pointer(dsdt_paddr))
					dsdt_paddr = fadt->dsdt;

				vaddr_t dsdt_vaddr = map_header(dsdt_paddr);
				if (dsdt_vaddr == 0)
					continue;

				MUST(m_mapped_headers.push_back({
					.paddr = dsdt_paddr,
					.vaddr = dsdt_vaddr
				}));
			}
		}

		return {};
	}

	const ACPI::SDTHeader* ACPI::get_header(BAN::StringView signature, uint32_t index)
	{
		if (signature.size() != 4)
		{
			dprintln("Trying to get ACPI header with {} byte signature ??", signature.size());
			return nullptr;
		}
		uint32_t cnt = 0;
		for (auto& mapped_header : m_mapped_headers)
		{
			auto* header = mapped_header.as_header();
			if (memcmp(header->signature, signature.data(), 4) == 0)
				if (cnt++ == index)
					return header;
		}
		return nullptr;
	}

}