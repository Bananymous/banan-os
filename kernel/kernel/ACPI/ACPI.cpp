#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML.h>
#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/Package.h>
#include <kernel/BootInfo.h>
#include <kernel/IO.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Timer/Timer.h>

#define RSPD_SIZE	20
#define RSPDv2_SIZE	36

namespace Kernel::ACPI
{


	static uint32_t* s_global_lock { nullptr };

	// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#global-lock
asm(R"(
.global acpi_acquire_global_lock
acpi_acquire_global_lock:
	movl (%rdi), %edx
	andl $(~1), %edx
	btsl $1, %edx
	adcl $0, %edx

	lock cmpxchgl %edx, (%rdi)
	jnz acpi_acquire_global_lock

	cmpb $3, %dl
	sbbq %rax, %rax
	negq %rax

	ret

.global acpi_release_global_lock
acpi_release_global_lock:
	movl (%rdi), %eax
	movl %eax, %edx

	andl $(~3), %edx

	lock cmpxchgl %edx, (%rdi)
	jnz acpi_release_global_lock

	andq $1, %rax

	ret
)");

	// returns true if lock was acquired successfully
	extern "C" bool acpi_acquire_global_lock(uint32_t* lock);

	// returns true if lock was pending
	extern "C" bool acpi_release_global_lock(uint32_t* lock);

	void ACPI::acquire_global_lock()
	{
		if (!s_global_lock)
			return;
		derrorln("Acquiring ACPI global lock");
		ASSERT(acpi_acquire_global_lock(s_global_lock));
	}

	void ACPI::release_global_lock()
	{
		if (!s_global_lock)
			return;
		derrorln("Releasing ACPI global lock");
		ASSERT(!acpi_release_global_lock(s_global_lock));
	}

	enum PM1Event : uint16_t
	{
		PM1_EVN_TMR_EN = 1 << 0,
		PM1_EVN_GBL_EN = 1 << 5,
		PM1_EVN_PWRBTN_EN = 1 << 8,
		PM1_EVN_SLPBTN_EN = 1 << 8,
		PM1_EVN_RTC_EN = 1 << 10,
		PM1_EVN_PCIEXP_WAKE_DIS = 1 << 14,
	};

	enum PM1Control : uint16_t
	{
		PM1_CNT_SCI_EN = 1 << 0,
		PM1_CNT_BM_RLD = 1 << 1,
		PM1_CNT_GBL_RLS = 1 << 2,
		PM1_CNT_SLP_EN = 1 << 13,

		PM1_CNT_SLP_TYP_MASK = 0b111,
		PM1_CNT_SLP_TYP_SHIFT = 10,
	};

	struct RSDT : public SDTHeader
	{
		uint32_t entries[];
	} __attribute__((packed));

	struct XSDT : public SDTHeader
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

		{
			ASSERT(!s_global_lock);
			const auto* fadt = static_cast<const FADT*>(ACPI::get().get_header("FACP"sv, 0));
			ASSERT(fadt);

			uintptr_t facs_addr = fadt->firmware_ctrl;
			if (fadt->length >= sizeof(FADT) && fadt->x_firmware_ctrl)
				facs_addr = fadt->x_firmware_ctrl;

			if (facs_addr)
			{
				auto* facs = reinterpret_cast<FACS*>(facs_addr);
				s_global_lock = &facs->global_lock;
			}
		}

		s_instance->m_namespace = AML::initialize_namespace();

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
		if (g_boot_info.rsdp.length)
			return &g_boot_info.rsdp;

		// Look in main BIOS area below 1 MB
		for (uintptr_t addr = P2V(0x000E0000); addr < P2V(0x000FFFFF); addr += 16)
			if (is_rsdp(addr))
				return reinterpret_cast<const RSDP*>(addr);
		return nullptr;
	}

	static bool is_valid_std_header(const SDTHeader* header)
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

		uint32_t root_entry_count = 0;

		if (rsdp->revision >= 2)
		{
			TRY(PageTable::with_fast_page(rsdp->xsdt_address & PAGE_ADDR_MASK,
				[&]() -> BAN::ErrorOr<void>
				{
					auto& xsdt = PageTable::fast_page_as<const XSDT>(rsdp->xsdt_address % PAGE_SIZE);
					if (memcmp(xsdt.signature, "XSDT", 4) != 0)
						return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);
					if (!is_valid_std_header(&xsdt))
						return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);

					m_header_table_paddr = rsdp->xsdt_address + offsetof(XSDT, entries);
					m_entry_size = 8;
					root_entry_count = (xsdt.length - sizeof(SDTHeader)) / 8;
					return {};
				}
			));
		}
		else
		{
			TRY(PageTable::with_fast_page(rsdp->rsdt_address & PAGE_ADDR_MASK,
				[&]() -> BAN::ErrorOr<void>
				{
					auto& rsdt = PageTable::fast_page_as<const RSDT>(rsdp->rsdt_address % PAGE_SIZE);
					if (memcmp(rsdt.signature, "RSDT", 4) != 0)
						return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);
					if (!is_valid_std_header(&rsdt))
						return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);

					m_header_table_paddr = rsdp->rsdt_address + offsetof(RSDT, entries);
					m_entry_size = 4;
					root_entry_count = (rsdt.length - sizeof(SDTHeader)) / 4;
					return {};
				}
			));
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
				size_t header_length;
				PageTable::with_fast_page(header_paddr & PAGE_ADDR_MASK, [&] {
					header_length = PageTable::fast_page_as<SDTHeader>(header_paddr % PAGE_SIZE).length;
				});

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

				paddr_t dsdt_paddr = 0;
				if (fadt->length > offsetof(FADT, x_dsdt))
					dsdt_paddr = fadt->x_dsdt;
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

	const SDTHeader* ACPI::get_header(BAN::StringView signature, uint32_t index)
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

	void ACPI::poweroff()
	{
		if (!m_namespace)
		{
			dwarnln("ACPI namespace not initialized");
			return;
		}

		auto s5_object = m_namespace->find_object({}, AML::NameString("\\_S5"));
		if (!s5_object)
		{
			dwarnln("\\_S5 not found");
			return;
		}
		auto s5_evaluated = s5_object->evaluate();
		if (!s5_evaluated)
		{
			dwarnln("Failed to evaluate \\_S5");
			return;
		}
		if (s5_evaluated->type != AML::Node::Type::Package)
		{
			dwarnln("\\_S5 is not a package");
			return;
		}
		auto* s5_package = static_cast<AML::Package*>(s5_evaluated.ptr());
		if (s5_package->elements.size() != 4)
		{
			dwarnln("\\_S5 package has {} elements, expected 4", s5_package->elements.size());
			return;
		}

		auto slp_typa = s5_package->elements[0]->as_integer();
		auto slp_typb = s5_package->elements[1]->as_integer();
		if (!slp_typa.has_value() || !slp_typb.has_value())
		{
			dwarnln("Failed to get SLP_TYPx values");
			return;
		}

		auto pts_object = m_namespace->find_object({}, AML::NameString("\\_PTS"));
		if (pts_object && pts_object->type == AML::Node::Type::Method)
		{
			auto* method = static_cast<AML::Method*>(pts_object.ptr());
			if (method->arg_count != 1)
			{
				dwarnln("Method \\_PTS has {} arguments, expected 1", method->arg_count);
				return;
			}

			AML::Method::Arguments args;
			args[0] = MUST(BAN::RefPtr<AML::Register>::create(MUST(BAN::RefPtr<AML::Integer>::create(5))));
			BAN::Vector<uint8_t> sync_stack;
			if (!method->evaluate(args, sync_stack).has_value())
			{
				dwarnln("Failed to evaluate \\_PTS");
				return;
			}

			dprintln("Executed \\_PTS");
		}

		dprintln("Entering sleep state S5");

		auto* fadt = static_cast<const FADT*>(get_header("FACP", 0));

		uint16_t pm1a_data = IO::inw(fadt->pm1a_cnt_blk);
		pm1a_data &= ~(PM1_CNT_SLP_TYP_MASK << PM1_CNT_SLP_TYP_SHIFT);
		pm1a_data |= (slp_typa.value() & PM1_CNT_SLP_TYP_MASK) << PM1_CNT_SLP_TYP_SHIFT;
		pm1a_data |= PM1_CNT_SLP_EN;
		IO::outw(fadt->pm1a_cnt_blk, pm1a_data);

		if (fadt->pm1b_cnt_blk != 0)
		{
			uint16_t pm1b_data = IO::inw(fadt->pm1b_cnt_blk);
			pm1b_data &= ~(PM1_CNT_SLP_TYP_MASK << PM1_CNT_SLP_TYP_SHIFT);
			pm1b_data |= (slp_typb.value() & PM1_CNT_SLP_TYP_MASK) << PM1_CNT_SLP_TYP_SHIFT;
			pm1b_data |= PM1_CNT_SLP_EN;
			IO::outw(fadt->pm1b_cnt_blk, pm1b_data);
		}
	}

	BAN::ErrorOr<void> ACPI::enter_acpi_mode(uint8_t mode)
	{
		if (!m_namespace)
		{
			dwarnln("ACPI namespace not initialized");
			return BAN::Error::from_errno(EFAULT);
		}

		// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/16_Waking_and_Sleeping/initialization.html#placing-the-system-in-acpi-mode
		auto* fadt = static_cast<const FADT*>(get_header("FACP", 0));

		// If not hardware-reduced ACPI and SCI_EN is not set
		if (!(fadt->flags & (1 << 20)) && IO::inw(fadt->pm1a_cnt_blk) & PM1_CNT_SCI_EN)
		{
			// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/04_ACPI_Hardware_Specification/ACPI_Hardware_Specification.html#legacy-acpi-select-and-the-sci-interrupt
			IO::outb(fadt->smi_cmd, fadt->acpi_enable);

			// Spec says to poll until SCI_EN is set, but doesn't specify timeout
			for (size_t i = 0; i < 100; i++)
			{
				if (IO::inw(fadt->pm1a_cnt_blk) & PM1_CNT_SCI_EN)
					break;
				SystemTimer::get().sleep(10);
			}

			if (!(IO::inw(fadt->pm1a_cnt_blk) & PM1_CNT_SCI_EN))
			{
				dwarnln("Failed to enable ACPI mode");
				return BAN::Error::from_errno(EINVAL);
			}

			// Enable power and sleep buttons
			IO::outw(fadt->pm1a_evt_blk + fadt->pm1_evt_len / 2, PM1_EVN_PWRBTN_EN | PM1_EVN_SLPBTN_EN);
			IO::outw(fadt->pm1b_evt_blk + fadt->pm1_evt_len / 2, PM1_EVN_PWRBTN_EN | PM1_EVN_SLPBTN_EN);
		}

		dprintln("Entered ACPI mode");

		dprintln("Initializing devices");

		// Evaluate \\_SB._INI
		auto _sb_ini = m_namespace->find_object({}, AML::NameString("\\_SB._INI"));
		if (_sb_ini && _sb_ini->type == AML::Node::Type::Method)
		{
			auto* method = static_cast<AML::Method*>(_sb_ini.ptr());
			if (method->arg_count != 0)
			{
				dwarnln("Method \\_SB._INI has {} arguments, expected 0", method->arg_count);
				return BAN::Error::from_errno(EINVAL);
			}
			BAN::Vector<uint8_t> sync_stack;
			method->evaluate({}, sync_stack);
		}

		// Initialize devices
		auto _sb = m_namespace->find_object({}, AML::NameString("\\_SB"));
		if (_sb && _sb->is_scope())
		{
			auto* scope = static_cast<AML::Scope*>(_sb.ptr());
			for (auto& [name, object] : scope->objects)
				if (object->type == AML::Node::Type::Device || object->type == AML::Node::Type::Processor)
					AML::initialize_device(object);
		}

		// Evaluate \\_PIC (mode)
		auto _pic = m_namespace->find_object({}, AML::NameString("\\_PIC"));
		if (_pic && _pic->type == AML::Node::Type::Method)
		{
			auto* method = static_cast<AML::Method*>(_pic.ptr());
			if (method->arg_count != 1)
			{
				dwarnln("Method \\_PIC has {} arguments, expected 1", method->arg_count);
				return BAN::Error::from_errno(EINVAL);
			}

			AML::Method::Arguments args;
			args[0] = MUST(BAN::RefPtr<AML::Register>::create(MUST(BAN::RefPtr<AML::Integer>::create(mode))));
			BAN::Vector<uint8_t> sync_stack;
			method->evaluate(args, sync_stack);
		}

		dprintln("Devices are initialized");

		return {};
	}

}
