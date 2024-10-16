#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML.h>
#include <kernel/ACPI/AML/Alias.h>
#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/Field.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/Package.h>
#include <kernel/BootInfo.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/Process.h>
#include <kernel/Timer/Timer.h>

#define RSPD_SIZE	20
#define RSPDv2_SIZE	36

namespace Kernel::ACPI
{

	static uint32_t* s_global_lock { nullptr };

	// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#global-lock
#if ARCH(x86_64)
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
#elif ARCH(i686)
asm(R"(
.global acpi_acquire_global_lock
acpi_acquire_global_lock:
	movl 4(%esp), %ecx
	movl (%ecx), %edx
	andl $(~1), %edx
	btsl $1, %edx
	adcl $0, %edx

	lock cmpxchgl %edx, (%ecx)
	jnz acpi_acquire_global_lock

	cmpb $3, %dl
	sbbl %eax, %eax
	negl %eax

	ret

.global acpi_release_global_lock
acpi_release_global_lock:
	movl 4(%esp), %ecx
	movl (%ecx), %eax
	movl %eax, %edx

	andl $(~3), %edx

	lock cmpxchgl %edx, (%ecx)
	jnz acpi_release_global_lock

	andl $1, %eax

	ret
)");
#endif

	// returns true if lock was acquired successfully
	extern "C" bool acpi_acquire_global_lock(uint32_t* lock);

	// returns true if lock was pending
	extern "C" bool acpi_release_global_lock(uint32_t* lock);

	void ACPI::acquire_global_lock()
	{
		if (!s_global_lock)
			return;
		ASSERT(acpi_acquire_global_lock(s_global_lock));
	}

	void ACPI::release_global_lock()
	{
		if (!s_global_lock)
			return;
		ASSERT(!acpi_release_global_lock(s_global_lock));
	}

	static BAN::Optional<AML::FieldRules::AccessType> get_access_type(uint8_t access_size)
	{
		switch (access_size)
		{
			case 0: return AML::FieldRules::AccessType::Any;
			case 1: return AML::FieldRules::AccessType::Byte;
			case 2: return AML::FieldRules::AccessType::Word;
			case 3: return AML::FieldRules::AccessType::DWord;
			case 4: return AML::FieldRules::AccessType::QWord;
			default:
				dwarnln("Unknown access size {}", access_size);
				return {};
		}
	}

	BAN::Optional<uint64_t> GAS::read()
	{
		auto access_type = get_access_type(access_size);
		if (!access_type.has_value())
			return {};

		auto op_region = MUST(BAN::RefPtr<AML::OpRegion>::create(""_sv, address_space_id, (uint64_t)address, 0xFFFFFFFF));

		auto field_rules = AML::FieldRules {
			.access_type = access_type.value(),
			.lock_rule = AML::FieldRules::LockRule::NoLock,
			.update_rule = AML::FieldRules::UpdateRule::Preserve,
			.access_attrib = AML::FieldRules::AccessAttrib::Normal,
			.access_length = 0
		};
		auto field_element = MUST(BAN::RefPtr<AML::FieldElement>::create(""_sv, register_bit_offset, register_bit_width, field_rules));
		field_element->op_region = op_region;

		auto result = field_element->convert(AML::Node::ConvInteger);
		if (!result)
			return {};
		return static_cast<AML::Integer*>(result.ptr())->value;
	}

	bool GAS::write(uint64_t value)
	{
		auto access_type = get_access_type(access_size);
		if (!access_type.has_value())
			return {};

		auto op_region = MUST(BAN::RefPtr<AML::OpRegion>::create(""_sv, address_space_id, (uint64_t)address, 0xFFFFFFFF));

		auto field_rules = AML::FieldRules {
			.access_type = access_type.value(),
			.lock_rule = AML::FieldRules::LockRule::NoLock,
			.update_rule = AML::FieldRules::UpdateRule::Preserve,
			.access_attrib = AML::FieldRules::AccessAttrib::Normal,
			.access_length = 0
		};
		auto field_element = MUST(BAN::RefPtr<AML::FieldElement>::create(""_sv, register_bit_offset, register_bit_width, field_rules));
		field_element->op_region = op_region;

		return !!field_element->store(MUST(BAN::RefPtr<AML::Integer>::create(value)));
	}

	enum PM1Event : uint16_t
	{
		PM1_EVN_TMR = 1 << 0,
		PM1_EVN_GBL = 1 << 5,
		PM1_EVN_PWRBTN = 1 << 8,
		PM1_EVN_SLPBTN = 1 << 8,
		PM1_EVN_RTC = 1 << 10,
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
			const auto* fadt = static_cast<const FADT*>(ACPI::get().get_header("FACP"_sv, 0));
			ASSERT(fadt);

			uintptr_t facs_addr = fadt->firmware_ctrl;
			if (fadt->length >= sizeof(FADT) && fadt->x_firmware_ctrl)
				facs_addr = fadt->x_firmware_ctrl;

			if (facs_addr)
			{
				size_t facs_size;
				PageTable::with_fast_page(facs_addr & PAGE_ADDR_MASK, [&] {
					facs_size = PageTable::fast_page_as<SDTHeader>(facs_addr % PAGE_SIZE).length;
				});

				size_t needed_pages = range_page_count(facs_addr, facs_size);
				vaddr_t facs_vaddr = PageTable::kernel().reserve_free_contiguous_pages(needed_pages, KERNEL_OFFSET);
				ASSERT(facs_vaddr);

				PageTable::kernel().map_range_at(
					facs_addr & PAGE_ADDR_MASK,
					facs_vaddr,
					needed_pages * PAGE_SIZE,
					PageTable::Flags::ReadWrite | PageTable::Flags::Present
				);

				auto* facs = reinterpret_cast<FACS*>(facs_vaddr + (facs_addr % PAGE_SIZE));
				s_global_lock = &facs->global_lock;
			}
		}

		return {};
	}

	ACPI& ACPI::get()
	{
		ASSERT(s_instance != nullptr);
		return *s_instance;
	}

	static bool is_rsdp(vaddr_t rsdp_addr)
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

	static BAN::Optional<RSDP> locate_rsdp()
	{
		if (g_boot_info.rsdp.length)
			return g_boot_info.rsdp;

		// Look in main BIOS area below 1 MB
		for (paddr_t paddr = 0x000E0000; paddr < 0x00100000; paddr += PAGE_SIZE)
		{
			BAN::Optional<RSDP> rsdp;

			PageTable::with_fast_page(paddr, [&rsdp] {
				for (size_t offset = 0; offset + sizeof(RSDP) <= PAGE_SIZE; offset += 16)
				{
					if (is_rsdp(PageTable::fast_page() + offset))
					{
						rsdp = PageTable::fast_page_as<RSDP>(offset);
						break;
					}
				}
			});

			if (rsdp.has_value())
				return rsdp.release_value();
		}

		return {};
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
		auto opt_rsdp = locate_rsdp();
		if (!opt_rsdp.has_value())
			return BAN::Error::from_error_code(ErrorCode::ACPI_NoRootSDT);
		const RSDP rsdp = opt_rsdp.release_value();

		uint32_t root_entry_count = 0;

		if (rsdp.revision >= 2)
		{
			TRY(PageTable::with_fast_page(rsdp.xsdt_address & PAGE_ADDR_MASK,
				[&]() -> BAN::ErrorOr<void>
				{
					auto& xsdt = PageTable::fast_page_as<const XSDT>(rsdp.xsdt_address % PAGE_SIZE);
					if (memcmp(xsdt.signature, "XSDT", 4) != 0)
						return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);
					if (!is_valid_std_header(&xsdt))
						return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);

					m_header_table_paddr = rsdp.xsdt_address + offsetof(XSDT, entries);
					m_entry_size = 8;
					root_entry_count = (xsdt.length - sizeof(SDTHeader)) / 8;
					return {};
				}
			));
		}
		else
		{
			TRY(PageTable::with_fast_page(rsdp.rsdt_address & PAGE_ADDR_MASK,
				[&]() -> BAN::ErrorOr<void>
				{
					auto& rsdt = PageTable::fast_page_as<const RSDT>(rsdp.rsdt_address % PAGE_SIZE);
					if (memcmp(rsdt.signature, "RSDT", 4) != 0)
						return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);
					if (!is_valid_std_header(&rsdt))
						return BAN::Error::from_error_code(ErrorCode::ACPI_RootInvalid);

					m_header_table_paddr = rsdp.rsdt_address + offsetof(RSDT, entries);
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

				m_fadt = fadt;
				m_hardware_reduced = fadt->flags & (1 << 20);
			}
		}

		if (m_fadt == nullptr)
			Kernel::panic("No FADT found");

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

	bool ACPI::prepare_sleep(uint8_t sleep_state)
	{
		auto pts_object = m_namespace->find_object({}, AML::NameString("_PTS"), AML::Namespace::FindMode::ForceAbsolute);
		if (pts_object && pts_object->type == AML::Node::Type::Method)
		{
			auto* method = static_cast<AML::Method*>(pts_object.ptr());
			if (method->arg_count != 1)
			{
				dwarnln("Method \\_PTS has {} arguments, expected 1", method->arg_count);
				return false;
			}

			if (!method->invoke(MUST(BAN::RefPtr<AML::Integer>::create(sleep_state))).has_value())
			{
				dwarnln("Failed to evaluate \\_PTS");
				return false;
			}

			dprintln("Executed \\_PTS");
		}

		return true;
	}

	void ACPI::poweroff()
	{
		if (!m_namespace)
		{
			dwarnln("ACPI namespace not initialized");
			return;
		}

		auto s5_object = m_namespace->find_object({}, AML::NameString("_S5"), AML::Namespace::FindMode::ForceAbsolute);
		if (!s5_object)
		{
			dwarnln("\\_S5 not found");
			return;
		}
		auto s5_evaluated = s5_object->to_underlying();
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
		if (s5_package->elements.size() < 2)
		{
			dwarnln("\\_S5 package has {} elements, expected atleast 2", s5_package->elements.size());
			return;
		}

		auto slp_typa_node = s5_package->elements[0]->convert(AML::Node::ConvInteger);
		auto slp_typb_node = s5_package->elements[1]->convert(AML::Node::ConvInteger);
		if (!slp_typa_node || !slp_typb_node)
		{
			dwarnln("Failed to get SLP_TYPx values");
			return;
		}

		if (!prepare_sleep(5))
			return;

		dprintln("Entering sleep state S5");

		const auto slp_typa_value = static_cast<AML::Integer*>(slp_typa_node.ptr())->value;
		const auto slp_typb_value = static_cast<AML::Integer*>(slp_typb_node.ptr())->value;

		uint16_t pm1a_data = IO::inw(fadt().pm1a_cnt_blk);
		pm1a_data &= ~(PM1_CNT_SLP_TYP_MASK << PM1_CNT_SLP_TYP_SHIFT);
		pm1a_data |= (slp_typa_value & PM1_CNT_SLP_TYP_MASK) << PM1_CNT_SLP_TYP_SHIFT;
		pm1a_data |= PM1_CNT_SLP_EN;
		IO::outw(fadt().pm1a_cnt_blk, pm1a_data);

		if (fadt().pm1b_cnt_blk != 0)
		{
			uint16_t pm1b_data = IO::inw(fadt().pm1b_cnt_blk);
			pm1b_data &= ~(PM1_CNT_SLP_TYP_MASK << PM1_CNT_SLP_TYP_SHIFT);
			pm1b_data |= (slp_typb_value & PM1_CNT_SLP_TYP_MASK) << PM1_CNT_SLP_TYP_SHIFT;
			pm1b_data |= PM1_CNT_SLP_EN;
			IO::outw(fadt().pm1b_cnt_blk, pm1b_data);
		}

		// system must not execute after sleep registers are written
		g_paniced = true;
		asm volatile("ud2");
	}

	void ACPI::reset()
	{
		// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/04_ACPI_Hardware_Specification/ACPI_Hardware_Specification.html#reset-register

		auto& reset_reg = fadt().reset_reg;
		switch (reset_reg.address_space_id)
		{
			case GAS::AddressSpaceID::SystemMemory:
			case GAS::AddressSpaceID::SystemIO:
			case GAS::AddressSpaceID::PCIConfig:
				break;
			default:
				dwarnln("Reset register has invalid address space ID ({})", static_cast<uint8_t>(reset_reg.address_space_id));
				return;
		}
		if (reset_reg.register_bit_offset != 0 || reset_reg.register_bit_width != 8)
		{
			dwarnln("Reset register has invalid location ({} bits at bit offset {})", reset_reg.register_bit_width, reset_reg.register_bit_offset);
			return;
		}

		if (!prepare_sleep(5))
			return;

		dprintln("Resetting system");

		if (!reset_reg.write(fadt().reset_value))
		{
			dwarnln("Could not write reset value");
			return;
		}

		// system must not execute after reset register is written
		g_paniced = true;
		asm volatile("ud2");
	}

	BAN::ErrorOr<void> ACPI::enter_acpi_mode(uint8_t mode)
	{
		ASSERT(!m_namespace);
		m_namespace = AML::initialize_namespace();
		if (!m_namespace)
			return BAN::Error::from_errno(EFAULT);

		// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/16_Waking_and_Sleeping/initialization.html#placing-the-system-in-acpi-mode

		// If not hardware-reduced ACPI and SCI_EN is not set
		if (!hardware_reduced() && !(IO::inw(fadt().pm1a_cnt_blk) & PM1_CNT_SCI_EN))
		{
			// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/04_ACPI_Hardware_Specification/ACPI_Hardware_Specification.html#legacy-acpi-select-and-the-sci-interrupt
			IO::outb(fadt().smi_cmd, fadt().acpi_enable);

			// Spec says to poll until SCI_EN is set, but doesn't specify timeout
			for (size_t i = 0; i < 100; i++)
			{
				if (IO::inw(fadt().pm1a_cnt_blk) & PM1_CNT_SCI_EN)
					break;
				SystemTimer::get().sleep_ms(10);
			}

			if (!(IO::inw(fadt().pm1a_cnt_blk) & PM1_CNT_SCI_EN))
			{
				dwarnln("Failed to enable ACPI mode");
				return BAN::Error::from_errno(EINVAL);
			}

			// Enable power and sleep buttons
			IO::outw(fadt().pm1a_evt_blk + fadt().pm1_evt_len / 2, PM1_EVN_PWRBTN | PM1_EVN_SLPBTN);
			IO::outw(fadt().pm1b_evt_blk + fadt().pm1_evt_len / 2, PM1_EVN_PWRBTN | PM1_EVN_SLPBTN);
		}

		dprintln("Entered ACPI mode");

		dprintln("Initializing devices");

		// Initialize \\_SB
		auto _sb = m_namespace->find_object({}, AML::NameString("_SB"), AML::Namespace::FindMode::ForceAbsolute);
		if (_sb && _sb->is_scope())
		{
			auto* scope = static_cast<AML::Scope*>(_sb.ptr());
			AML::initialize_scope(scope);
		}

		// Evaluate \\_PIC (mode)
		auto _pic = m_namespace->find_object({}, AML::NameString("_PIC"), AML::Namespace::FindMode::ForceAbsolute);
		if (_pic && _pic->type == AML::Node::Type::Method)
		{
			auto* method = static_cast<AML::Method*>(_pic.ptr());
			if (method->arg_count != 1)
			{
				dwarnln("Method \\_PIC has {} arguments, expected 1", method->arg_count);
				return BAN::Error::from_errno(EINVAL);
			}
			method->invoke(MUST(BAN::RefPtr<AML::Integer>::create(mode)));
		}

		dprintln("Devices are initialized");

		uint8_t irq = fadt().sci_int;
		if (auto ret = InterruptController::get().reserve_irq(irq); ret.is_error())
			dwarnln("Could not enable ACPI interrupt: {}", ret.error());
		else
		{
			auto hex_sv_to_int =
				[](BAN::StringView sv) -> BAN::Optional<uint32_t>
				{
					uint32_t ret = 0;
					for (char c : sv)
					{
						ret <<= 4;
						if (c >= '0' && c <= '9')
							ret += c - '0';
						else if (c >= 'A' && c <= 'F')
							ret += c - 'A' + 10;
						else if (c >= 'a' && c <= 'f')
							ret += c - 'a' + 10;
						else
							return {};
					}
					return ret;
				};

			if (fadt().gpe0_blk)
			{
				// Enable all events in _GPE (_Lxx or _Exx)
				m_namespace->for_each_child(AML::NameString("\\_GPE"),
					[&](const auto& path, auto& node)
					{
						if (node->type != AML::Node::Type::Method)
							return;
						if (path.size() < 4)
							return;

						auto name = path.sv().substring(path.size() - 4);
						if (name.substring(0, 2) != "_L"_sv && name.substring(0, 2) != "_E"_sv)
							return;

						auto index = hex_sv_to_int(name.substring(2));
						if (!index.has_value())
							return;

						auto byte = index.value() / 8;
						auto bit = index.value() % 8;
						auto gpe0_en_port = fadt().gpe0_blk + (fadt().gpe0_blk_len / 2) + byte;
						IO::outb(gpe0_en_port, IO::inb(gpe0_en_port) | (1 << bit));

						auto* method = static_cast<AML::Method*>(node.ptr());
						m_gpe_methods[index.value()] = method;

						dprintln("Enabled GPE {}", index.value(), byte, bit);
					}
				);
			}

			set_irq(irq);
			InterruptController::get().enable_irq(irq);

			Process::create_kernel([](void*) { get().acpi_event_task(); }, nullptr);
		}

		return {};
	}

	void ACPI::acpi_event_task()
	{
		auto get_fixed_event =
			[&](uint16_t sts_port)
			{
				if (sts_port == 0)
					return 0;
				auto sts = IO::inw(sts_port);
				auto en = IO::inw(sts_port + fadt().pm1_evt_len / 2);
				if (auto pending = sts & en)
					return pending & ~(pending - 1);
				return 0;
			};

		while (true)
		{
			uint16_t sts_port;
			uint16_t pending;

			sts_port = fadt().pm1a_evt_blk;
			if (pending = get_fixed_event(sts_port); pending)
				goto handle_event;

			sts_port = fadt().pm1b_evt_blk;
			if (pending = get_fixed_event(sts_port); pending)
				goto handle_event;

			{
				bool handled_event = false;
				uint8_t gpe0_bytes = fadt().gpe0_blk_len / 2;
				for (uint8_t i = 0; i < gpe0_bytes; i++)
				{
					uint8_t sts = IO::inb(fadt().gpe0_blk + i);
					uint8_t en = IO::inb(fadt().gpe0_blk + gpe0_bytes + i);
					pending = sts & en;
					if (pending == 0)
						continue;

					auto index = i * 8 + (pending & ~(pending - 1));
					if (m_gpe_methods[index])
						m_gpe_methods[index]->invoke();

					handled_event = true;
					IO::outb(fadt().gpe0_blk + i, 1 << index);
				}
				if (handled_event)
					continue;
			}


			// FIXME: this can cause missing of event if it happens between
			//        reading the status and blocking
			m_event_thread_blocker.block_with_timeout_ms(100);
			continue;

handle_event:
			if (pending & PM1_EVN_PWRBTN)
			{
				dprintln("Power button pressed");
				if (auto ret = Process::clean_poweroff(POWEROFF_SHUTDOWN); ret.is_error())
					dwarnln("Failed to poweroff: {}", ret.error());
			}
			else
			{
				dwarnln("Unhandled ACPI fixed event {H}", pending);
			}

			IO::outw(sts_port, pending);
		}
	}

	void ACPI::handle_irq()
	{
		m_event_thread_blocker.unblock();
	}

}
