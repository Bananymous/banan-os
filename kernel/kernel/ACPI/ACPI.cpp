#include <BAN/ScopeGuard.h>
#include <BAN/StringView.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML/OpRegion.h>
#include <kernel/ACPI/BatterySystem.h>
#include <kernel/BootInfo.h>
#include <kernel/InterruptController.h>
#include <kernel/IO.h>
#include <kernel/Lock/SpinLockAsMutex.h>
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

	static BAN::ErrorOr<uint8_t> get_access_type(uint8_t access_size)
	{
		switch (access_size)
		{
			case 0: return 0;
			case 1: return 1;
			case 2: return 2;
			case 3: return 3;
			case 4: return 4;
			default:
				dwarnln("Unknown access size {}", access_size);
				return BAN::Error::from_errno(EFAULT);
		}
	}

	BAN::ErrorOr<uint64_t> GAS::read()
	{
		AML::OpRegion opregion;
		opregion.address_space = address_space_id;
		opregion.offset = address;
		opregion.length = 0xFFFFFFFF;

		AML::Node field_unit;
		field_unit.type = AML::Node::Type::FieldUnit;
		field_unit.as.field_unit.type = AML::FieldUnit::Type::Field;
		field_unit.as.field_unit.as.field.opregion = opregion;
		field_unit.as.field_unit.length = register_bit_width;
		field_unit.as.field_unit.offset = register_bit_offset;
		field_unit.as.field_unit.flags = TRY(get_access_type(access_size));

		auto result = TRY(AML::convert_from_field_unit(field_unit, AML::ConvInteger, sizeof(uint64_t)));
		return result.as.integer.value;
	}

	BAN::ErrorOr<void> GAS::write(uint64_t value)
	{
		AML::OpRegion opregion;
		opregion.address_space = address_space_id;
		opregion.offset = address;
		opregion.length = 0xFFFFFFFF;

		AML::Node field_unit;
		field_unit.type = AML::Node::Type::FieldUnit;
		field_unit.as.field_unit.type = AML::FieldUnit::Type::Field;
		field_unit.as.field_unit.as.field.opregion = opregion;
		field_unit.as.field_unit.length = register_bit_width;
		field_unit.as.field_unit.offset = register_bit_offset;
		field_unit.as.field_unit.flags = TRY(get_access_type(access_size));

		AML::Node source;
		source.type = AML::Node::Type::Integer;
		source.as.integer.value = value;

		TRY(AML::store_to_field_unit(source, field_unit));

		return {};
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

	BAN::ErrorOr<void> ACPI::prepare_sleep(uint8_t sleep_state)
	{
		if (!m_namespace)
			return BAN::Error::from_errno(EFAULT);

		auto [pts_path, pts_object] = TRY(m_namespace->find_named_object({}, MUST(AML::NameString::from_string("\\_PTS"))));
		if (pts_object == nullptr)
			return {};

		auto& pts_node = pts_object->node;
		if (pts_node.type != AML::Node::Type::Method)
		{
			dwarnln("Object \\_PTS is not a method");
			return BAN::Error::from_errno(EFAULT);
		}

		if (pts_node.as.method.arg_count != 1)
		{
			dwarnln("Method \\_PTS has {} arguments, expected 1", pts_node.as.method.arg_count);
			return BAN::Error::from_errno(EFAULT);
		}

		AML::Reference arg_ref;
		arg_ref.node.type = AML::Node::Type::Integer;
		arg_ref.node.as.integer.value = sleep_state;
		arg_ref.ref_count = 2;

		BAN::Array<AML::Reference*, 7> arguments(nullptr);
		arguments[0] = &arg_ref; // method call should not delete argument
		TRY(AML::method_call(pts_path, pts_node, BAN::move(arguments)));

		dprintln("Executed \\_PTS({})", sleep_state);

		return {};
	}

	BAN::ErrorOr<void> ACPI::poweroff()
	{
		if (!m_namespace)
		{
			dwarnln("ACPI namespace not initialized");
			return BAN::Error::from_errno(EFAULT);
		}

		auto [_, s5_object] = TRY(m_namespace->find_named_object({}, TRY(AML::NameString::from_string("\\_S5_"_sv))));
		if (!s5_object)
		{
			dwarnln("\\_S5 not found");
			return BAN::Error::from_errno(EFAULT);
		}

		auto& s5_node = s5_object->node;
		if (s5_node.type != AML::Node::Type::Package)
		{
			dwarnln("\\_S5 is not a package");
			return BAN::Error::from_errno(EFAULT);
		}
		if (s5_node.as.package->num_elements < 2)
		{
			dwarnln("\\_S5 package has {} elements, expected atleast 2", s5_node.as.package->num_elements);
			return BAN::Error::from_errno(EFAULT);
		}

		if (!s5_node.as.package->elements[0].resolved || !s5_node.as.package->elements[1].resolved)
		{
			dwarnln("TODO: lazy evaluate package \\_S5 elements");
			return BAN::Error::from_errno(ENOTSUP);
		}

		auto slp_typa_node = TRY(AML::convert_node(TRY(s5_node.as.package->elements[0].value.node->copy()), AML::ConvInteger, sizeof(uint64_t)));
		auto slp_typb_node = TRY(AML::convert_node(TRY(s5_node.as.package->elements[1].value.node->copy()), AML::ConvInteger, sizeof(uint64_t)));

		TRY(prepare_sleep(5));

		dprintln("Entering sleep state S5");

		const auto slp_typa_value = slp_typa_node.as.integer.value;
		const auto slp_typb_value = slp_typb_node.as.integer.value;

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
		panic("ACPI shutdown failed. You can now safely shutdown your computer.");
	}

	BAN::ErrorOr<void> ACPI::reset()
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
				return BAN::Error::from_errno(EFAULT);
		}

		if (reset_reg.register_bit_offset != 0 || reset_reg.register_bit_width != 8)
		{
			dwarnln("Reset register has invalid location ({} bits at bit offset {})", reset_reg.register_bit_width, reset_reg.register_bit_offset);
			return BAN::Error::from_errno(EFAULT);
		}

		if (!m_namespace)
			dwarnln("ACPI namespace not initialized, will not evaluate \\_S5");
		else
			TRY(prepare_sleep(5));

		dprintln("Resetting system");

		TRY(reset_reg.write(fadt().reset_value));

		// system must not execute after reset register is written
		ASSERT_NOT_REACHED();
	}

	BAN::ErrorOr<void> ACPI::load_aml_tables(BAN::StringView name, bool all)
	{
		BAN::ErrorOr<void> result {};

		for (uint32_t i = 0;; i++)
		{
			auto* header = get_header(name, i);
			if (header == nullptr)
				break;

			if (all)
				dprintln("Parsing {}{}, {} bytes", name, i + 1, header->length);
			else
				dprintln("Parsing {}, {} bytes", name, header->length);

			auto header_span = BAN::ConstByteSpan(reinterpret_cast<const uint8_t*>(header), header->length);
			if (auto parse_ret = m_namespace->parse(header_span); parse_ret.is_error())
				result = parse_ret.release_error();

			if (!all)
				break;
		}

		return result;
	}

#pragma GCC diagnostic push
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wstack-usage="
#endif
	BAN::ErrorOr<void> ACPI::route_interrupt_link_device(const AML::Scope& device, uint64_t& routed_irq_mask)
	{
		ASSERT(m_namespace);

		auto prs_node = TRY(AML::convert_node(TRY(m_namespace->evaluate(device, "_PRS"_sv)), AML::ConvBuffer, -1));
		auto prs_span = BAN::ConstByteSpan(prs_node.as.str_buf->bytes, prs_node.as.str_buf->size);

		auto [srs_path, srs_node] = TRY(m_namespace->find_named_object(device, TRY(AML::NameString::from_string("_SRS"_sv)), true));
		if (srs_node == nullptr || srs_node->node.type != AML::Node::Type::Method)
		{
			dwarnln("interrupt link device does not have _SRS method");
			return BAN::Error::from_errno(EINVAL);
		}

		while (!prs_span.empty())
		{
			if (!(prs_span[0] & 0x80))
			{
				const uint8_t name = (prs_span[0] >> 3) & 0x0F;
				const uint8_t length = prs_span[0] & 0x07;
				if (prs_span.size() < static_cast<size_t>(1 + length))
					return BAN::Error::from_errno(EINVAL);

				if (name == 0x04)
				{
					if (length < 2)
						return BAN::Error::from_errno(EINVAL);
					const uint16_t irq_mask = prs_span[1] | (prs_span[2] << 8);

					for (uint8_t pass = 0; pass < 2; pass++)
					{
						for (uint8_t irq = 0; irq < 16; irq++)
						{
							if (!(irq_mask & (1 << irq)))
								continue;
							if (pass == 0 && (routed_irq_mask & (static_cast<uint64_t>(1) << irq)))
								continue;

							BAN::Array<uint8_t, 4> setting;
							setting[0] = 0x22 | (length > 2); // small, irq, data len
							setting[1] = (1 << irq) >> 0;     // irq low
							setting[2] = (1 << irq) >> 8;     // irq high
							if (length > 2)
								setting[3] = prs_span[3]; // flags

							auto setting_span = BAN::ConstByteSpan(setting.data(), (length > 2) ? 4 : 3);
							TRY(AML::method_call(srs_path, srs_node->node, TRY(AML::Node::create_buffer(setting_span))));

							dprintln("routed {} -> irq {}", device, irq);

							routed_irq_mask |= static_cast<uint64_t>(1) << irq;
							return {};
						}
					}
				}

				prs_span = prs_span.slice(1 + length);
			}
			else
			{
				if (prs_span.size() < 3)
					return BAN::Error::from_errno(EINVAL);
				const uint8_t name = prs_span[0] & 0x7F;
				const uint16_t length = (prs_span[2] << 8) | prs_span[1];
				if (prs_span.size() < static_cast<size_t>(3 + length))
					return BAN::Error::from_errno(EINVAL);

				// Extended Interrupt Descriptor
				if (name == 0x09)
				{
					const uint8_t irq_count = prs_span[4];
					if (irq_count == 0 || length < 2 + 4*irq_count)
						return BAN::Error::from_errno(EINVAL);

					for (uint8_t pass = 0; pass < 2; pass++)
					{
						for (uint32_t i = 0; i < irq_count; i++)
						{
							// TODO: support irq over 64 irqs?
							if (prs_span[6 + 4*i] || prs_span[7 + 4*i] || prs_span[8 + 4*i])
								continue;
							const uint8_t irq = prs_span[5 + 4*i];
							if (irq >= 64)
								continue;
							if (pass == 0 && (routed_irq_mask & (static_cast<uint64_t>(1) << irq)))
								continue;

							BAN::Array<uint8_t, 9> setting;
							setting[0] = 0x89;            // large, irq
							setting[1] = 0x06;            // data len
							setting[2] = 0x00;
							setting[3] = prs_span[3]; // flags
							setting[4] = 0x01;            // table size
							setting[5] = irq;             // irq
							setting[6] = 0x00;
							setting[7] = 0x00;
							setting[8] = 0x00;

							TRY(AML::method_call(srs_path, srs_node->node, TRY(AML::Node::create_buffer(setting.span()))));

							dprintln("routed {} -> irq {}", device, irq);

							routed_irq_mask |= static_cast<uint64_t>(1) << irq;
							return {};
						}
					}
				}

				prs_span = prs_span.slice(3 + length);
			}
		}

		dwarnln("No routable interrupt found in _PRS");
		return {};
	}
#pragma GCC diagnostic pop

	BAN::Optional<GAS> ACPI::find_gpe_block(size_t index)
	{
#define FIND_GPE(idx)                                                             \
			{                                                                     \
				const uint8_t null[sizeof(GAS)] {};                               \
				if (fadt().length > offsetof(FADT, x_gpe##idx##_blk)              \
					&& memcmp(fadt().x_gpe##idx##_blk, null, sizeof(GAS)) == 0) { \
					auto gas = *reinterpret_cast<GAS*>(fadt().x_gpe##idx##_blk);  \
					if (gas.address != 0) {                                       \
						gas.register_bit_width = 8;                               \
						gas.access_size = 1;                                      \
						if (!gas.read().is_error())                               \
							return gas;                                           \
					}                                                             \
				}                                                                 \
                                                                                  \
				if (fadt().gpe##idx##_blk) {                                      \
					return GAS {                                                  \
						.address_space_id = GAS::AddressSpaceID::SystemIO,        \
						.register_bit_width = 8,                                  \
						.register_bit_offset = 0,                                 \
						.access_size = 1,                                         \
						.address = fadt().gpe##idx##_blk,                         \
					};                                                            \
				}                                                                 \
				return {};                                                        \
			}

		switch (index)
		{
			case 0: FIND_GPE(0);
			case 1: FIND_GPE(1);
			default: ASSERT_NOT_REACHED();
		}
#undef FIND_GPE
	}

	BAN::ErrorOr<void> ACPI::initialize_embedded_controller(const AML::Scope& embedded_controller)
	{
		BAN::Optional<uint8_t> gpe_int;

		do {
			auto [gpe_path, gpe_obj] = TRY(m_namespace->find_named_object(embedded_controller, TRY(AML::NameString::from_string("_GPE"_sv)), true));
			if (gpe_obj == nullptr)
			{
				dwarnln("EC {} does have _GPE", embedded_controller);
				break;
			}

			auto gpe = TRY(AML::evaluate_node(gpe_path, gpe_obj->node));
			if (gpe.type == AML::Node::Type::Package)
			{
				dwarnln("TODO: EC {} has package _GPE");
				break;
			}

			gpe_int = TRY(AML::convert_node(BAN::move(gpe), AML::ConvInteger, -1)).as.integer.value;
		} while (false);

		auto [crs_path, crs_obj] = TRY(m_namespace->find_named_object(embedded_controller, TRY(AML::NameString::from_string("_CRS"_sv)), true));
		if (crs_obj == nullptr)
		{
			dwarnln("EC {} does have _CRS", embedded_controller);
			return BAN::Error::from_errno(ENOENT);
		}

		const auto crs = TRY(AML::evaluate_node(crs_path, crs_obj->node));
		if (crs.type != AML::Node::Type::Buffer)
		{
			dwarnln("EC {} _CRS is not a buffer, but {}", embedded_controller, crs);
			return BAN::Error::from_errno(EINVAL);
		}

		const auto extract_io_port =
			[](BAN::ConstByteSpan& buffer) -> BAN::ErrorOr<uint16_t>
			{
				if (buffer.empty())
					return BAN::Error::from_errno(ENODATA);

				uint16_t result;
				bool decode_16;

				switch (buffer[0])
				{
					case 0x47: // IO Port Descriptor
						if (buffer.size() < 8)
							return BAN::Error::from_errno(ENODATA);
						decode_16 = !!(buffer[1] & (1 << 0));
						result = (buffer[3] << 8) | buffer[2];
						buffer = buffer.slice(8);
						break;
					case 0x4B: // Fixed Location IO Port Descriptor
						if (buffer.size() < 4)
							return BAN::Error::from_errno(ENODATA);
						decode_16 = false;
						result = (buffer[2] << 8) | buffer[1];
						buffer = buffer.slice(4);
						break;
					default:
						dwarnln("EC _CRS has unhandled resouce descriptor 0x{2H}", buffer[0]);
						return BAN::Error::from_errno(EINVAL);
				}

				const uint16_t mask = decode_16 ? 0xFFFF : 0x03FF;
				return result & mask;
			};

		// TODO: EC can also reside in memory space
		auto crs_buffer = BAN::ConstByteSpan { crs.as.str_buf->bytes, static_cast<size_t>(crs.as.str_buf->size) };
		const auto data_port = TRY(extract_io_port(crs_buffer));
		const auto command_port = TRY(extract_io_port(crs_buffer));

		TRY(m_embedded_controllers.push_back(TRY(EmbeddedController::create(TRY(embedded_controller.copy()), command_port, data_port, gpe_int))));
		return {};
	}

	BAN::ErrorOr<void> ACPI::initialize_embedded_controllers()
	{
		auto embedded_controllers = TRY(m_namespace->find_device_with_eisa_id("PNP0C09"));

		for (auto& embedded_controller : embedded_controllers)
			if (auto ret = initialize_embedded_controller(embedded_controller); ret.is_error())
				dwarnln("Failed to initialize embedded controller: {}", ret.error());

		dprintln("Initialized {}/{} embedded controllers",
			m_embedded_controllers.size(),
			embedded_controllers.size()
		);

		return {};
	}

	BAN::ErrorOr<void> ACPI::register_gpe_handler(uint8_t gpe, void (*callback)(void*), void* argument)
	{
		if (m_gpe_methods[gpe].method)
			return BAN::Error::from_errno(EEXIST);

		m_gpe_methods[gpe].has_callback = true;
		m_gpe_methods[gpe] = {
			.has_callback = true,
			.callback = callback,
			.argument = argument,
		};

		if (!enable_gpe(gpe))
		{
			m_gpe_methods[gpe] = {};
			return BAN::Error::from_errno(EFAULT);
		}

		dprintln("Enabled _GPE {}", gpe);

		return {};
	}

	bool ACPI::enable_gpe(uint8_t gpe)
	{
		const auto enable_gpe_impl =
			[](const GAS& gpe_block, size_t gpe, size_t base, size_t blk_len) -> bool
			{
				if (gpe < base || gpe >= base + blk_len / 2 * 8)
					return false;
				const auto byte = (gpe - base) / 8;
				const auto bit  = (gpe - base) % 8;
				auto enabled = ({ auto tmp = gpe_block; tmp.address += (blk_len / 2) + byte; tmp; });
				MUST(enabled.write(MUST(enabled.read()) | (1 << bit)));
				return true;
			};

		const auto gpe0 = find_gpe_block(0);
		const size_t gpe0_base = 0;
		const size_t gpe0_blk_len = gpe0.has_value() ? fadt().gpe0_blk_len : 0;
		if (gpe0.has_value() && enable_gpe_impl(gpe0.value(), gpe, gpe0_base, gpe0_blk_len))
		{
			m_has_any_gpes = true;
			return true;
		}

		const auto gpe1 = find_gpe_block(1);
		const size_t gpe1_base = fadt().gpe1_base;
		const size_t gpe1_blk_len = gpe1.has_value() ? fadt().gpe1_blk_len : 0;
		if (gpe1.has_value() && enable_gpe_impl(gpe1.value(), gpe, gpe1_base, gpe1_blk_len))
		{
			m_has_any_gpes = true;
			return true;
		}

		return false;
	}

	BAN::ErrorOr<void> ACPI::enter_acpi_mode(uint8_t mode)
	{
		ASSERT(!m_namespace);

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

		TRY(AML::Namespace::prepare_root_namespace());
		m_namespace = &AML::Namespace::root_namespace();

		if (auto ret = load_aml_tables("DSDT"_sv, false); ret.is_error())
			dwarnln("Could not load DSDT: {}", ret.error());
		if (auto ret = load_aml_tables("SSDT"_sv, true); ret.is_error())
			dwarnln("Could not load all SSDTs: {}", ret.error());
		if (auto ret = load_aml_tables("PSDT"_sv, true); ret.is_error())
			dwarnln("Could not load all PSDTs: {}", ret.error());

		dprintln("Loaded ACPI tables");

		{
			const auto disable_gpe_block =
				[](const GAS& gpe, size_t blk_len) {
					for (size_t i = 0; i < blk_len / 2; i++)
						MUST(({ auto tmp = gpe; tmp.address += blk_len / 2 + i; tmp; }).write(0));
				};

			if (auto gpe0 = find_gpe_block(0); gpe0.has_value())
				disable_gpe_block(gpe0.value(), fadt().gpe0_blk_len);

			if (auto gpe1 = find_gpe_block(1); gpe1.has_value())
				disable_gpe_block(gpe1.value(), fadt().gpe1_blk_len);

			// FIXME: add support for GPE blocks inside the ACPI namespace
		}

		if (auto ret = initialize_embedded_controllers(); ret.is_error())
			dwarnln("Failed to initialize Embedded Controllers: {}", ret.error());

		if (auto ret = m_namespace->post_load_initialize(); ret.is_error())
			dwarnln("Failed to initialize ACPI namespace: {}", ret.error());

		auto [pic_path, pic_obj] = TRY(m_namespace->find_named_object({}, TRY(AML::NameString::from_string("\\_PIC"_sv))));
		if (pic_obj && pic_obj->node.type == AML::Node::Type::Method)
		{
			auto& pic_node = pic_obj->node;
			if (pic_node.as.method.arg_count != 1)
			{
				dwarnln("Method \\_PIC has {} arguments, expected 1", pic_node.as.method.arg_count);
				return BAN::Error::from_errno(EINVAL);
			}

			AML::Reference arg_ref;
			arg_ref.node.type = AML::Node::Type::Integer;
			arg_ref.node.as.integer.value = mode;
			arg_ref.ref_count = 2;

			BAN::Array<AML::Reference*, 7> arguments(nullptr);
			arguments[0] = &arg_ref; // method call should not delete argument
			TRY(AML::method_call(pic_path, pic_node, BAN::move(arguments)));
		}

		dprintln("Evaluated \\_PIC({})", mode);

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

			auto [gpe_scope, gpe_obj] = TRY(m_namespace->find_named_object({}, TRY(AML::NameString::from_string("\\_GPE"))));
			if (gpe_obj && gpe_obj->node.is_scope())
			{
				m_gpe_scope = BAN::move(gpe_scope);

				// Enable all events in _GPE (_Lxx or _Exx)
				TRY(m_namespace->for_each_child(m_gpe_scope,
					[&](BAN::StringView name, AML::Reference* node_ref) -> BAN::Iteration
					{
						if (node_ref->node.type != AML::Node::Type::Method)
							return BAN::Iteration::Continue;

						ASSERT(name.size() == 4);
						if (!name.starts_with("_L"_sv) && !name.starts_with("_E"_sv))
							return BAN::Iteration::Continue;

						auto opt_index = hex_sv_to_int(name.substring(2));
						if (!opt_index.has_value())
						{
							dwarnln("invalid GPE number '{}'", name);
							return BAN::Iteration::Continue;
						}

						const auto index = opt_index.value();
						if (enable_gpe(index))
						{
							m_gpe_methods[index] = {
								.has_callback = false,
								.method = node_ref
							};
							node_ref->ref_count++;
							dprintln("Enabled {}", name);
						}

						return BAN::Iteration::Continue;
					}
				));
			}

			set_irq(irq);
			InterruptController::get().enable_irq(irq);

			if (auto thread_or_error = Thread::create_kernel([](void*) { get().acpi_event_task(); }, nullptr); thread_or_error.is_error())
				dwarnln("Failed to create ACPI thread, power button will not work: {}", thread_or_error.error());
			else if (auto ret = Processor::scheduler().add_thread(thread_or_error.value()); ret.is_error())
				dwarnln("Failed to create ACPI thread, power button will not work: {}", ret.error());
		}

		dprintln("Initialized ACPI interrupts");

		if (auto interrupt_link_devices_or_error = m_namespace->find_device_with_eisa_id("PNP0C0F"_sv); !interrupt_link_devices_or_error.is_error())
		{
			uint64_t routed_irq_mask = 0;
			auto interrupt_link_devices = interrupt_link_devices_or_error.release_value();
			for (const auto& device : interrupt_link_devices)
				if (auto ret = route_interrupt_link_device(device, routed_irq_mask); ret.is_error())
					dwarnln("failed to route interrupt link device: {}", ret.error());
			dprintln("Routed interrupt link devices");
		}

		return {};
	}

	BAN::ErrorOr<void> ACPI::initialize_acpi_devices()
	{
		ASSERT(m_namespace);
		TRY(BatterySystem::initialize(*m_namespace));
		return {};
	}

	void ACPI::acpi_event_task()
	{
		const auto get_fixed_event =
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

		const auto try_handle_gpe = [this](GAS gpe_blk, uint8_t gpe_blk_len, uint32_t base) -> bool {
			bool handled = false;
			for (uint8_t i = 0; i < gpe_blk_len / 2; i++)
			{
				auto status  = ({ auto tmp = gpe_blk; tmp.address +=                     i; tmp; });
				auto enabled = ({ auto tmp = gpe_blk; tmp.address += (gpe_blk_len / 2) + i; tmp; });
				const uint8_t pending = MUST(status.read()) & MUST(enabled.read());
				if (pending == 0)
					continue;

				for (size_t bit = 0; bit < 8; bit++)
				{
					if (!(pending & (1 << bit)))
						continue;

					const auto gpe = base + i * 8 + bit;
					if (auto& method = m_gpe_methods[gpe]; method.method == nullptr)
						dwarnln("No handler for _GPE {}", gpe);
					else
					{
						if (method.has_callback)
							method.callback(method.argument);
						else if (auto ret = AML::method_call(m_gpe_scope, method.method->node, BAN::Array<AML::Reference*, 7>{}); ret.is_error())
							dwarnln("Failed to evaluate _GPE {}: ", gpe, ret.error());
						else
							dprintln("handled _GPE {}", gpe);
					}
				}

				MUST(status.write(pending));
				handled = true;
			}

			return handled;
		};

		const auto gpe0 = m_has_any_gpes ? find_gpe_block(0) : BAN::Optional<GAS>{};
		const auto gpe1 = m_has_any_gpes ? find_gpe_block(1) : BAN::Optional<GAS>{};

		while (true)
		{
			uint16_t sts_port;
			uint16_t pending;

			sts_port = fadt().pm1a_evt_blk;
			if (sts_port && (pending = get_fixed_event(sts_port)))
				goto handle_event;

			sts_port = fadt().pm1b_evt_blk;
			if (sts_port && (pending = get_fixed_event(sts_port)))
				goto handle_event;

			if (gpe0.has_value() && try_handle_gpe(gpe0.value(), fadt().gpe0_blk_len, 0))
				continue;

			if (gpe1.has_value() && try_handle_gpe(gpe1.value(), fadt().gpe1_blk_len, fadt().gpe1_base))
				continue;

			// FIXME: this can cause missing of event if it happens between
			//        reading the status and blocking
			m_event_thread_blocker.block_with_timeout_ms(100, nullptr);
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
