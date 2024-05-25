#include <BAN/ScopeGuard.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML/Field.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/IO.h>
#include <kernel/PCI.h>

namespace Kernel::ACPI
{

	template<typename Func>
	concept ReadFunc = requires(Func func, uint64_t offset)
	{
		requires BAN::is_same_v<decltype(func(offset)), BAN::Optional<uint64_t>>;
	};

	template<typename Func>
	concept WriteFunc = requires(Func func, uint64_t offset, uint64_t value)
	{
		requires BAN::is_same_v<decltype(func(offset, value)), bool>;
	};

	template<typename Element>
	struct ParseFieldElementContext
	{
		AML::FieldRules										field_rules;
		uint64_t											field_bit_offset;
		BAN::ConstByteSpan									field_pkg;
		BAN::HashMap<AML::NameSeg, BAN::RefPtr<Element>>	elements;
	};

	template<typename Element>
	static bool parse_field_element(ParseFieldElementContext<Element>& context)
	{
		// FIXME: Validate elements

		ASSERT(context.field_pkg.size() >= 1);
		switch (context.field_pkg[0])
		{
			case 0x00:
			{
				context.field_pkg = context.field_pkg.slice(1);

				auto reserved_length = AML::parse_pkg_length(context.field_pkg);
				if (!reserved_length.has_value())
				{
					AML_ERROR("Invalid FieldElement length for reserved field");
					return false;
				}
				AML::trim_pkg_length(context.field_pkg);

				context.field_bit_offset += reserved_length.value();
				return true;
			}
			case 0x01:
			{
				context.field_pkg = context.field_pkg.slice(1);

				if (context.field_pkg.size() < 2)
				{
					AML_ERROR("Invalid FieldElement length for access field");
					return false;
				}

				context.field_rules.access_type = static_cast<AML::FieldRules::AccessType>(context.field_pkg[0] & 0x0F);
				context.field_rules.access_attrib = static_cast<AML::FieldRules::AccessAttrib>((context.field_pkg[0] >> 6) & 0x03);
				context.field_pkg = context.field_pkg.slice(1);

				context.field_rules.access_length = context.field_pkg[0];
				context.field_pkg = context.field_pkg.slice(1);

				return true;
			}
			case 0x02:
				AML_TODO("Field element Connection", context.field_pkg[0]);
				return false;
			case 0x03:
			{
				context.field_pkg = context.field_pkg.slice(1);

				if (context.field_pkg.size() < 3)
				{
					AML_ERROR("Invalid FieldElement length for extended access field");
					return false;
				}

				context.field_rules.access_type = static_cast<AML::FieldRules::AccessType>(context.field_pkg[0] & 0x0F);
				context.field_rules.lock_rule = static_cast<AML::FieldRules::LockRule>((context.field_pkg[0] >> 4) & 0x01);
				context.field_rules.update_rule = static_cast<AML::FieldRules::UpdateRule>((context.field_pkg[0] >> 5) & 0x03);
				context.field_pkg = context.field_pkg.slice(1);

				if (context.field_pkg[0] == 0x0B)
					context.field_rules.access_attrib = AML::FieldRules::AccessAttrib::Bytes;
				else if (context.field_pkg[0] == 0x0E)
					context.field_rules.access_attrib = AML::FieldRules::AccessAttrib::RawBytes;
				else if (context.field_pkg[0] == 0x0F)
					context.field_rules.access_attrib = AML::FieldRules::AccessAttrib::RawProcessBytes;
				else
				{
					AML_ERROR("Invalid FieldElement extended access field attribute");
					return false;
				}
				context.field_pkg = context.field_pkg.slice(1);

				context.field_rules.access_length = context.field_pkg[0];
				context.field_pkg = context.field_pkg.slice(1);

				return true;
			}
			default:
			{
				auto element_name = AML::NameSeg::parse(context.field_pkg);
				if (!element_name.has_value())
				{
					AML_ERROR("Invalid FieldElement name for named field");
					return false;
				}

				auto element_length = AML::parse_pkg_length(context.field_pkg);
				if (!element_length.has_value())
				{
					AML_ERROR("Invalid FieldElement length for named field");
					return false;
				}
				AML::trim_pkg_length(context.field_pkg);

				if (context.elements.contains(element_name.value()))
				{
					AML_ERROR("Field element already exists");
					return false;
				}

				MUST(context.elements.emplace(
					element_name.value(),
					MUST(BAN::RefPtr<Element>::create(
						element_name.value(),
						context.field_bit_offset,
						element_length.value(),
						context.field_rules
					))
				));
				context.field_bit_offset += element_length.value();

				return true;
			}
		}
	}

	static BAN::Optional<uint32_t> determine_access_size(const AML::FieldRules::AccessType& access_type)
	{
		switch (access_type)
		{
			case AML::FieldRules::AccessType::Any:
			case AML::FieldRules::AccessType::Byte:
				return 1;
			case AML::FieldRules::AccessType::Word:
				return 2;
			case AML::FieldRules::AccessType::DWord:
				return 4;
			case AML::FieldRules::AccessType::QWord:
				return 8;
			case AML::FieldRules::AccessType::Buffer:
				AML_TODO("FieldElement with access type Buffer");
				return {};
		}
		return {};
	}

	static BAN::Optional<uint64_t> perform_read(AML::OpRegion::RegionSpace region_space, uint64_t access_offset, uint32_t access_size)
	{
		switch (region_space)
		{
			case AML::OpRegion::RegionSpace::SystemMemory:
			{
				uint64_t result = 0;
				size_t index_in_page = (access_offset % PAGE_SIZE) / access_size;
				PageTable::with_fast_page(access_offset & PAGE_ADDR_MASK, [&] {
					switch (access_size)
					{
						case 1: result = PageTable::fast_page_as_sized<uint8_t> (index_in_page); break;
						case 2: result = PageTable::fast_page_as_sized<uint16_t>(index_in_page); break;
						case 4: result = PageTable::fast_page_as_sized<uint32_t>(index_in_page); break;
						case 8: result = PageTable::fast_page_as_sized<uint64_t>(index_in_page); break;
					}
				});
				return result;
			}
			case AML::OpRegion::RegionSpace::SystemIO:
			{
				uint64_t result = 0;
				switch (access_size)
				{
					case 1: result = IO::inb(access_offset); break;
					case 2: result = IO::inw(access_offset); break;
					case 4: result = IO::inl(access_offset); break;
					default:
						AML_ERROR("FieldElement read_field (SystemIO) with access size {}", access_size);
						return {};
				}
				return result;
			}
			case AML::OpRegion::RegionSpace::PCIConfig:
			{
				// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#address-space-format
				// PCI configuration space is confined to segment 0, bus 0

				uint16_t device		= (access_offset >> 32) & 0xFFFF;
				uint16_t function	= (access_offset >> 16) & 0xFFFF;
				uint16_t offset		= access_offset & 0xFFFF;

				uint64_t result = 0;
				switch (access_size)
				{
					case 1: result = PCI::PCIManager::get().read_config_byte(0, device, function, offset); break;
					case 2: result = PCI::PCIManager::get().read_config_word(0, device, function, offset); break;
					case 4: result = PCI::PCIManager::get().read_config_dword(0, device, function, offset); break;
					default:
						AML_ERROR("FieldElement read_field (PCIConfig) with access size {}", access_size);
						return {};
				}
				return result;
			}
			default:
				AML_TODO("FieldElement read_field with region space {}", static_cast<uint8_t>(region_space));
				return {};
		}
	}

	static bool perform_write(AML::OpRegion::RegionSpace region_space, uint64_t access_offset, uint32_t access_size, uint64_t value)
	{
		switch (region_space)
		{
			case AML::OpRegion::RegionSpace::SystemMemory:
			{
				size_t index_in_page = (access_offset % PAGE_SIZE) / access_size;
				PageTable::with_fast_page(access_offset & PAGE_ADDR_MASK, [&] {
					switch (access_size)
					{
						case 1: PageTable::fast_page_as_sized<uint8_t> (index_in_page) = value; break;
						case 2: PageTable::fast_page_as_sized<uint16_t>(index_in_page) = value; break;
						case 4: PageTable::fast_page_as_sized<uint32_t>(index_in_page) = value; break;
						case 8: PageTable::fast_page_as_sized<uint64_t>(index_in_page) = value; break;
					}
				});
				return true;
			}
			case AML::OpRegion::RegionSpace::SystemIO:
			{
				switch (access_size)
				{
					case 1: IO::outb(access_offset, value); break;
					case 2: IO::outw(access_offset, value); break;
					case 4: IO::outl(access_offset, value); break;
					default:
						AML_ERROR("FieldElement write_field (SystemIO) with access size {}", access_size);
						return false;
				}
				return true;
			}
			case AML::OpRegion::RegionSpace::PCIConfig:
			{
				// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#address-space-format
				// PCI configuration space is confined to segment 0, bus 0

				uint16_t device		= (access_offset >> 32) & 0xFFFF;
				uint16_t function	= (access_offset >> 16) & 0xFFFF;
				uint16_t offset		= access_offset & 0xFFFF;

				switch (access_size)
				{
					case 1: PCI::PCIManager::get().write_config_byte(0, device, function, offset, value); break;
					case 2: PCI::PCIManager::get().write_config_word(0, device, function, offset, value); break;
					case 4: PCI::PCIManager::get().write_config_dword(0, device, function, offset, value); break;
					default:
						AML_ERROR("FieldElement write_field (PCIConfig) with access size {}", access_size);
						return false;
				}
				return true;
			}
			default:
				AML_TODO("FieldElement write_field with region space {}", static_cast<uint8_t>(region_space));
				return false;
		}
	}

	template<ReadFunc ReadFunc>
	static BAN::Optional<uint64_t> perform_read_general(
		uint64_t base_byte_offset,
		uint64_t bit_count,
		uint64_t bit_offset,
		uint32_t access_size,
		ReadFunc& read_func
	)
	{
		if (bit_count > 64)
		{
			AML_TODO("Field read with bit_count > 64");
			return {};
		}

		uint32_t access_bytes = access_size;
		uint32_t access_bits = access_bytes * 8;

		uint64_t result = 0;
		uint32_t bits_read = 0;
		while (bits_read < bit_count)
		{
			uint64_t byte_offset = base_byte_offset + ((bit_offset + bits_read) / 8);
			if (auto rem = byte_offset % access_bytes)
				byte_offset -= rem;

			auto partial = read_func(byte_offset);
			if (!partial.has_value())
				return {};

			uint32_t shift = (bit_offset + bits_read) % access_bits;
			uint32_t valid_bits = BAN::Math::min<uint32_t>(access_bits - shift, bit_count - bits_read);
			uint64_t mask = ((uint64_t)1 << valid_bits) - 1;

			result |= ((partial.value() >> shift) & mask) << bits_read;
			bits_read += valid_bits;
		}

		return result;
	}

	template<ReadFunc ReadFunc, WriteFunc WriteFunc>
	static bool perform_write_general(
		uint64_t base_byte_offset,
		uint64_t bit_count,
		uint64_t bit_offset,
		uint32_t access_size,
		uint64_t value,
		AML::FieldRules::UpdateRule update_rule,
		ReadFunc& read_func,
		WriteFunc& write_func
	)
	{
		if (bit_count > 64)
		{
			AML_TODO("Field write with bit_count > 64");
			return false;
		}

		uint32_t access_bytes = access_size;
		uint32_t access_bits = access_bytes * 8;

		uint32_t bits_written = 0;
		while (bits_written < bit_count)
		{
			uint64_t byte_offset = base_byte_offset + ((bit_offset + bits_written) / 8);
			if (auto rem = byte_offset % access_bytes)
				byte_offset -= rem;

			uint32_t shift = (bit_offset + bits_written) % access_bits;
			uint32_t valid_bits = BAN::Math::min<uint32_t>(access_bits - shift, bit_count - bits_written);
			uint64_t mask = ((uint64_t)1 << valid_bits) - 1;

			uint64_t to_write = 0;
			if (valid_bits != access_bits)
			{
				switch (update_rule)
				{
					case AML::FieldRules::UpdateRule::Preserve:
					{
						auto read_result = read_func(byte_offset);
						if (!read_result.has_value())
							return false;
						to_write = read_result.value() & ~(mask << shift);
						break;
					}
					case AML::FieldRules::UpdateRule::WriteAsOnes:
						to_write = ~(mask << shift);
						break;
					case AML::FieldRules::UpdateRule::WriteAsZeros:
						to_write = 0;
						break;
				}
			}
			to_write |= ((value >> bits_written) & mask) << shift;

			if (!write_func(byte_offset, to_write))
				return false;

			bits_written += valid_bits;
		}

		return true;
	}

	AML::ParseResult AML::Field::parse(ParseContext& context)
	{
		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::FieldOp);
		context.aml_data = context.aml_data.slice(2);

		auto opt_field_pkg = AML::parse_pkg(context.aml_data);
		if (!opt_field_pkg.has_value())
			return ParseResult::Failure;
		auto field_pkg = opt_field_pkg.release_value();

		auto name_string = NameString::parse(field_pkg);
		if (!name_string.has_value())
			return ParseResult::Failure;

		auto op_region = Namespace::root_namespace()->find_object(context.scope, name_string.value(), Namespace::FindMode::Normal);
		if (!op_region || op_region->type != AML::Node::Type::OpRegion)
		{
			AML_ERROR("FieldOp: {} does not name a valid OpRegion", name_string.value());
			return ParseResult::Failure;
		}

		if (field_pkg.size() < 1)
			return ParseResult::Failure;
		auto field_flags = field_pkg[0];
		field_pkg = field_pkg.slice(1);

		ParseFieldElementContext<FieldElement> field_context;
		field_context.field_rules.access_type = static_cast<FieldRules::AccessType>(field_flags & 0x0F);
		field_context.field_rules.lock_rule = static_cast<FieldRules::LockRule>((field_flags >> 4) & 0x01);
		field_context.field_rules.update_rule = static_cast<FieldRules::UpdateRule>((field_flags >> 5) & 0x03);
		field_context.field_bit_offset = 0;
		field_context.field_pkg = field_pkg;
		while (field_context.field_pkg.size() > 0)
			if (!parse_field_element(field_context))
				return ParseResult::Failure;

		for (auto& [_, element] : field_context.elements)
		{
			element->op_region = static_cast<OpRegion*>(op_region.ptr());

			NameString element_name;
			MUST(element_name.path.push_back(element->name));
			if (!Namespace::root_namespace()->add_named_object(context, element_name, element))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			element->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif
		}

		return ParseResult::Success;
	}

	BAN::Optional<uint64_t> AML::FieldElement::evaluate_internal()
	{
		if (access_rules.access_attrib != FieldRules::AccessAttrib::Normal)
		{
			AML_TODO("FieldElement with access attribute {}", static_cast<uint8_t>(access_rules.access_attrib));
			return {};
		}
		auto access_size = determine_access_size(access_rules.access_type);
		if (!access_size.has_value())
			return {};
		auto read_func = [&](uint64_t byte_offset) -> BAN::Optional<uint64_t> {
			return perform_read(op_region->region_space, byte_offset, access_size.value());
		};
		return perform_read_general(op_region->region_offset, bit_count, bit_offset, access_size.value(), read_func);
	}

	bool AML::FieldElement::store_internal(uint64_t value)
	{
		if (access_rules.access_attrib != FieldRules::AccessAttrib::Normal)
		{
			AML_TODO("FieldElement with access attribute {}", static_cast<uint8_t>(access_rules.access_attrib));
			return {};
		}
		auto access_size = determine_access_size(access_rules.access_type);
		if (!access_size.has_value())
			return false;
		auto read_func = [&](uint64_t byte_offset) -> BAN::Optional<uint64_t> {
			return perform_read(op_region->region_space, byte_offset, access_size.value());
		};
		auto write_func = [&](uint64_t byte_offset, uint64_t value) -> bool {
			return perform_write(op_region->region_space, byte_offset, access_size.value(), value);
		};
		return perform_write_general(op_region->region_offset, bit_count, bit_offset, access_size.value(), value, access_rules.update_rule, read_func, write_func);
	}

	BAN::RefPtr<AML::Node> AML::FieldElement::evaluate()
	{
		op_region->mutex.lock();
		BAN::ScopeGuard unlock_guard([&] {
			op_region->mutex.unlock();
		});

		auto result = evaluate_internal();
		if (!result.has_value())
			return {};
		return MUST(BAN::RefPtr<Integer>::create(result.value()));
	}

	bool AML::FieldElement::store(BAN::RefPtr<AML::Node> source)
	{
		auto source_integer = source->as_integer();
		if (!source_integer.has_value())
		{
			AML_TODO("FieldElement store with non-integer source, type {}", static_cast<uint8_t>(source->type));
			return false;
		}

		op_region->mutex.lock();
		if (access_rules.lock_rule == FieldRules::LockRule::Lock)
			ACPI::acquire_global_lock();
		BAN::ScopeGuard unlock_guard([&] {
			op_region->mutex.unlock();
			if (access_rules.lock_rule == FieldRules::LockRule::Lock)
				ACPI::release_global_lock();
		});

		return store_internal(source_integer.value());
	}

	void AML::FieldElement::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("FieldElement {} ({}, offset {}, OpRegion {})",
			name,
			bit_count,
			bit_offset,
			op_region->name
		);
	}

	AML::ParseResult AML::IndexField::parse(ParseContext& context)
	{
		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::IndexFieldOp);
		context.aml_data = context.aml_data.slice(2);

		auto opt_field_pkg = AML::parse_pkg(context.aml_data);
		if (!opt_field_pkg.has_value())
			return ParseResult::Failure;
		auto field_pkg = opt_field_pkg.release_value();

		auto index_field_element_name = NameString::parse(field_pkg);
		if (!index_field_element_name.has_value())
			return ParseResult::Failure;
		auto index_field_element = Namespace::root_namespace()->find_object(context.scope, index_field_element_name.value(), Namespace::FindMode::Normal);
		if (!index_field_element || index_field_element->type != AML::Node::Type::FieldElement)
		{
			AML_ERROR("IndexField IndexName does not name a valid FieldElement");
			return ParseResult::Failure;
		}

		auto data_field_element_name = NameString::parse(field_pkg);
		if (!data_field_element_name.has_value())
			return ParseResult::Failure;
		auto data_field_element = Namespace::root_namespace()->find_object(context.scope, data_field_element_name.value(), Namespace::FindMode::Normal);
		if (!data_field_element || data_field_element->type != AML::Node::Type::FieldElement)
		{
			AML_ERROR("IndexField DataName does not name a valid FieldElement");
			return ParseResult::Failure;
		}

		if (field_pkg.size() < 1)
			return ParseResult::Failure;
		auto field_flags = field_pkg[0];
		field_pkg = field_pkg.slice(1);

		ParseFieldElementContext<IndexFieldElement> field_context;
		field_context.field_rules.access_type = static_cast<FieldRules::AccessType>(field_flags & 0x0F);
		field_context.field_rules.lock_rule = static_cast<FieldRules::LockRule>((field_flags >> 4) & 0x01);
		field_context.field_rules.update_rule = static_cast<FieldRules::UpdateRule>((field_flags >> 5) & 0x03);
		field_context.field_bit_offset = 0;
		field_context.field_pkg = field_pkg;
		while (field_context.field_pkg.size() > 0)
			if (!parse_field_element(field_context))
				return ParseResult::Failure;

		for (auto& [_, element] : field_context.elements)
		{
			element->index_element = static_cast<FieldElement*>(index_field_element.ptr());
			element->data_element = static_cast<FieldElement*>(data_field_element.ptr());

			NameString element_name;
			MUST(element_name.path.push_back(element->name));
			if (!Namespace::root_namespace()->add_named_object(context, element_name, element))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			element->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif
		}

		return AML::ParseResult::Success;
	}

	BAN::RefPtr<AML::Node> AML::IndexFieldElement::evaluate()
	{
		if (access_rules.access_attrib != FieldRules::AccessAttrib::Normal)
		{
			AML_TODO("IndexFieldElement with access attribute {}", static_cast<uint8_t>(access_rules.access_attrib));
			return {};
		}
		auto access_size = determine_access_size(access_rules.access_type);
		if (!access_size.has_value())
			return {};
		if (access_size.value() > data_element->bit_count)
		{
			AML_ERROR("IndexFieldElement read_field with access size {} > data element bit count {}", access_size.value(), data_element->bit_count);
			return {};
		}
		auto read_func = [&](uint64_t byte_offset) -> BAN::Optional<uint64_t> {
			if (!index_element->store_internal(byte_offset))
				return {};
			return data_element->evaluate_internal();
		};

		index_element->op_region->mutex.lock();
		if (access_rules.lock_rule == FieldRules::LockRule::Lock)
			ACPI::acquire_global_lock();
		BAN::ScopeGuard unlock_guard([&] {
			index_element->op_region->mutex.unlock();
			if (access_rules.lock_rule == FieldRules::LockRule::Lock)
				ACPI::release_global_lock();
		});

		auto result = perform_read_general(0, bit_count, bit_offset, access_size.value(), read_func);
		if (!result.has_value())
			return {};
		return MUST(BAN::RefPtr<Integer>::create(result.value()));
	}

	bool AML::IndexFieldElement::store(BAN::RefPtr<Node> source)
	{
		if (access_rules.access_attrib != FieldRules::AccessAttrib::Normal)
		{
			AML_TODO("FieldElement with access attribute {}", static_cast<uint8_t>(access_rules.access_attrib));
			return {};
		}
		auto source_integer = source->as_integer();
		if (!source_integer.has_value())
		{
			AML_TODO("IndexFieldElement store with non-integer source, type {}", static_cast<uint8_t>(source->type));
			return false;
		}

		auto access_size = determine_access_size(access_rules.access_type);
		if (!access_size.has_value())
			return false;

		if (access_size.value() > data_element->bit_count)
		{
			AML_ERROR("IndexFieldElement write_field with access size {} > data element bit count {}", access_size.value(), data_element->bit_count);
			return false;
		}

		auto read_func = [&](uint64_t byte_offset) -> BAN::Optional<uint64_t> {
			if (!index_element->store_internal(byte_offset))
				return {};
			return data_element->evaluate_internal();
		};
		auto write_func = [&](uint64_t byte_offset, uint64_t value) -> bool {
			if (!index_element->store_internal(byte_offset))
				return false;
			return data_element->store_internal(value);
		};

		index_element->op_region->mutex.lock();
		if (access_rules.lock_rule == FieldRules::LockRule::Lock)
			ACPI::acquire_global_lock();
		BAN::ScopeGuard unlock_guard([&] {
			index_element->op_region->mutex.unlock();
			if (access_rules.lock_rule == FieldRules::LockRule::Lock)
				ACPI::release_global_lock();
		});

		if (!perform_write_general(0, bit_count, bit_offset, access_size.value(), source_integer.value(), access_rules.update_rule, read_func, write_func))
			return false;

		return true;
	}

	void AML::IndexFieldElement::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("IndexFieldElement {} ({}, offset {}, IndexName {}, DataName {})",
			name,
			bit_count,
			bit_offset,
			index_element->name,
			data_element->name
		);
	}

	AML::ParseResult AML::BankField::parse(ParseContext& context)
	{
		// BankFieldOp PkgLength NameString NameString BankValue FieldFlags FieldList

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::BankFieldOp);
		context.aml_data = context.aml_data.slice(2);

		auto opt_field_pkg = AML::parse_pkg(context.aml_data);
		if (!opt_field_pkg.has_value())
			return ParseResult::Failure;
		auto field_pkg = opt_field_pkg.release_value();

		auto op_region_name = NameString::parse(field_pkg);
		if (!op_region_name.has_value())
			return ParseResult::Failure;
		auto op_region = Namespace::root_namespace()->find_object(context.scope, op_region_name.value(), Namespace::FindMode::Normal);
		if (!op_region || op_region->type != AML::Node::Type::OpRegion)
		{
			AML_ERROR("BankField RegionName {} does not name a valid OpRegion", op_region_name.value());
			return ParseResult::Failure;
		}

		auto bank_selector_name = NameString::parse(field_pkg);
		if (!bank_selector_name.has_value())
			return ParseResult::Failure;
		auto bank_selector = Namespace::root_namespace()->find_object(context.scope, bank_selector_name.value(), Namespace::FindMode::Normal);
		if (!bank_selector)
		{
			AML_ERROR("BankField BankSelector {} does not name a valid object", bank_selector_name.value());
			return ParseResult::Failure;
		}
		if (bank_selector->type != AML::Node::Type::FieldElement)
		{
			AML_TODO("BankField BankSelector {} type {2H}", static_cast<uint8_t>(bank_selector->type));
			return ParseResult::Failure;
		}

		auto temp_aml_data = context.aml_data;
		context.aml_data = field_pkg;
		auto bank_value_result = AML::parse_object(context);
		field_pkg = context.aml_data;
		context.aml_data = temp_aml_data;
		if (!bank_value_result.success())
			return ParseResult::Failure;
		auto bank_value = bank_value_result.node() ? bank_value_result.node()->as_integer() : BAN::Optional<uint64_t>();
		if (!bank_value.has_value())
		{
			AML_ERROR("BankField BankValue is not an integer");
			return ParseResult::Failure;
		}

		if (field_pkg.size() < 1)
			return ParseResult::Failure;
		auto field_flags = field_pkg[0];
		field_pkg = field_pkg.slice(1);

		ParseFieldElementContext<BankFieldElement> field_context;
		field_context.field_rules.access_type = static_cast<FieldRules::AccessType>(field_flags & 0x0F);
		field_context.field_rules.lock_rule = static_cast<FieldRules::LockRule>((field_flags >> 4) & 0x01);
		field_context.field_rules.update_rule = static_cast<FieldRules::UpdateRule>((field_flags >> 5) & 0x03);
		field_context.field_bit_offset = 0;
		field_context.field_pkg = field_pkg;
		while (field_context.field_pkg.size() > 0)
			if (!parse_field_element(field_context))
				return ParseResult::Failure;

		for (auto& [_, element] : field_context.elements)
		{
			element->op_region = static_cast<OpRegion*>(op_region.ptr());
			element->bank_selector = static_cast<FieldElement*>(bank_selector.ptr());
			element->bank_value = bank_value.value();

			NameString element_name;
			MUST(element_name.path.push_back(element->name));
			if (!Namespace::root_namespace()->add_named_object(context, element_name, element))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			element->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif
		}

		return ParseResult::Success;
	}

	BAN::RefPtr<AML::Node> AML::BankFieldElement::evaluate()
	{
		if (access_rules.access_attrib != FieldRules::AccessAttrib::Normal)
		{
			AML_TODO("BankFieldElement with access attribute {}", static_cast<uint8_t>(access_rules.access_attrib));
			return {};
		}

		auto access_size = determine_access_size(access_rules.access_type);
		if (!access_size.has_value())
			return {};
		auto read_func = [&](uint64_t byte_offset) -> BAN::Optional<uint64_t> {
			return perform_read(op_region->region_space, byte_offset, access_size.value());
		};

		bank_selector->op_region->mutex.lock();
		if (access_rules.lock_rule == FieldRules::LockRule::Lock)
			ACPI::acquire_global_lock();
		BAN::ScopeGuard unlock_guard([&] {
			bank_selector->op_region->mutex.unlock();
			if (access_rules.lock_rule == FieldRules::LockRule::Lock)
				ACPI::release_global_lock();
		});

		if (!bank_selector->store_internal(bank_value))
		{
			AML_ERROR("BankFieldElement failed to store BankValue");
			return {};
		}

		auto result = perform_read_general(op_region->region_offset, bit_count, bit_offset, access_size.value(), read_func);
		if (!result.has_value())
			return {};
		return MUST(BAN::RefPtr<Integer>::create(result.value()));
	}

	bool AML::BankFieldElement::store(BAN::RefPtr<AML::Node> source)
	{
		if (access_rules.access_attrib != FieldRules::AccessAttrib::Normal)
		{
			AML_TODO("BankFieldElement with access attribute {}", static_cast<uint8_t>(access_rules.access_attrib));
			return {};
		}

		auto source_integer = source->as_integer();
		if (!source_integer.has_value())
		{
			AML_TODO("BankFieldElement store with non-integer source, type {}", static_cast<uint8_t>(source->type));
			return false;
		}

		auto access_size = determine_access_size(access_rules.access_type);
		if (!access_size.has_value())
			return false;
		auto read_func = [&](uint64_t byte_offset) -> BAN::Optional<uint64_t> {
			return perform_read(op_region->region_space, byte_offset, access_size.value());
		};
		auto write_func = [&](uint64_t byte_offset, uint64_t value) -> bool {
			return perform_write(op_region->region_space, byte_offset, access_size.value(), value);
		};

		bank_selector->op_region->mutex.lock();
		if (access_rules.lock_rule == FieldRules::LockRule::Lock)
			ACPI::acquire_global_lock();
		BAN::ScopeGuard unlock_guard([&] {
			bank_selector->op_region->mutex.unlock();
			if (access_rules.lock_rule == FieldRules::LockRule::Lock)
				ACPI::release_global_lock();
		});

		if (!bank_selector->store_internal(bank_value))
		{
			AML_ERROR("BankFieldElement failed to store BankValue");
			return {};
		}

		return perform_write_general(op_region->region_offset, bit_count, bit_offset, access_size.value(), source_integer.value(), access_rules.update_rule, read_func, write_func);
	}

	void AML::BankFieldElement::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("BankFieldElement {} ({}, offset {}, OpRegion {}, BankSelector {}, BankValue {H})",
			name,
			bit_count,
			bit_offset,
			op_region->name,
			bank_selector->name,
			bank_value
		);
	}

}
