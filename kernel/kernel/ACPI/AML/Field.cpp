#include <BAN/ScopeGuard.h>
#include <kernel/ACPI/AML/Field.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/IO.h>
#include <kernel/PCI.h>

namespace Kernel::ACPI
{

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
			case 0x02:
			case 0x03:
				AML_TODO("Field element {2H}", context.field_pkg[0]);
				return false;
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

		auto op_region = Namespace::root_namespace()->find_object(context.scope, name_string.value());
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


	BAN::Optional<AML::FieldElement::AccessType> AML::FieldElement::determine_access_type() const
	{
		uint32_t access_size = 0;
		switch (access_rules.access_type)
		{
			case FieldRules::AccessType::Any:
			case FieldRules::AccessType::Byte:
				access_size = 1;
				break;
			case FieldRules::AccessType::Word:
				access_size = 2;
				break;
			case FieldRules::AccessType::DWord:
				access_size = 4;
				break;
			case FieldRules::AccessType::QWord:
				access_size = 8;
				break;
			case FieldRules::AccessType::Buffer:
				AML_TODO("FieldElement with access type Buffer");
				return {};
		}

		uint64_t byte_offset = op_region->region_offset + (bit_offset / 8);
		if (auto rem = byte_offset % access_size)
			byte_offset -= rem;

		if ((bit_offset % access_size) + bit_count > access_size * 8)
		{
			AML_ERROR("FieldElement over multiple access sizes");
			return {};
		}

		if (byte_offset + access_size > op_region->region_offset + op_region->region_length)
		{
			AML_ERROR("FieldElement out of bounds");
			return {};
		}

		uint32_t shift = bit_offset % access_size;
		uint64_t mask = ((uint64_t)1 << bit_count) - 1;

		return AccessType {
			.offset = byte_offset,
			.mask = mask,
			.access_size = access_size,
			.shift = shift
		};
	}

	BAN::Optional<uint64_t> AML::FieldElement::read_field(uint64_t access_offset, uint32_t access_size) const
	{
		switch (op_region->region_space)
		{
			case OpRegion::RegionSpace::SystemMemory:
			{
				uint64_t result = 0;
				PageTable::with_fast_page(access_offset & PAGE_ADDR_MASK, [&] {
					switch (access_size)
					{
						case 1: result = PageTable::fast_page_as_sized<uint8_t> ((access_offset % PAGE_SIZE) / access_size); break;
						case 2: result = PageTable::fast_page_as_sized<uint16_t>((access_offset % PAGE_SIZE) / access_size); break;
						case 4: result = PageTable::fast_page_as_sized<uint32_t>((access_offset % PAGE_SIZE) / access_size); break;
						case 8: result = PageTable::fast_page_as_sized<uint64_t>((access_offset % PAGE_SIZE) / access_size); break;
					}
				});
				return result;
			}
			case OpRegion::RegionSpace::SystemIO:
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
			case OpRegion::RegionSpace::PCIConfig:
			{
				// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#address-space-format
				// PCI configuration space is confined to segment 0, bus 0

				uint16_t device		= (access_offset >> 32) & 0xFFFF;
				uint16_t function	= (access_offset >> 16) & 0xFFFF;
				uint16_t offset		= access_offset & 0xFFFF;

				uint64_t result = 0;
				switch (access_size)
				{
					case 1: result = PCI::PCIManager::read_config_byte(0, device, function, offset); break;
					case 2: result = PCI::PCIManager::read_config_word(0, device, function, offset); break;
					case 4: result = PCI::PCIManager::read_config_dword(0, device, function, offset); break;
					default:
						AML_ERROR("FieldElement read_field (PCIConfig) with access size {}", access_size);
						return {};
				}
				return result;
			}
			default:
				AML_TODO("FieldElement read_field with region space {}", static_cast<uint8_t>(op_region->region_space));
				return {};
		}
	}

	bool AML::FieldElement::write_field(uint64_t access_offset, uint32_t access_size, uint64_t value) const
	{
		switch (op_region->region_space)
		{
			case OpRegion::RegionSpace::SystemMemory:
			{
				PageTable::with_fast_page(access_offset & PAGE_ADDR_MASK, [&] {
					switch (access_size)
					{
						case 1: PageTable::fast_page_as_sized<uint8_t> ((access_offset % PAGE_SIZE) / access_size) = value; break;
						case 2: PageTable::fast_page_as_sized<uint16_t>((access_offset % PAGE_SIZE) / access_size) = value; break;
						case 4: PageTable::fast_page_as_sized<uint32_t>((access_offset % PAGE_SIZE) / access_size) = value; break;
						case 8: PageTable::fast_page_as_sized<uint64_t>((access_offset % PAGE_SIZE) / access_size) = value; break;
					}
				});
				return true;
			}
			case OpRegion::RegionSpace::SystemIO:
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
			case OpRegion::RegionSpace::PCIConfig:
			{
				// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#address-space-format
				// PCI configuration space is confined to segment 0, bus 0

				uint16_t device		= (access_offset >> 32) & 0xFFFF;
				uint16_t function	= (access_offset >> 16) & 0xFFFF;
				uint16_t offset		= access_offset & 0xFFFF;

				switch (access_size)
				{
					case 1: PCI::PCIManager::write_config_byte(0, device, function, offset, value); break;
					case 2: PCI::PCIManager::write_config_word(0, device, function, offset, value); break;
					case 4: PCI::PCIManager::write_config_dword(0, device, function, offset, value); break;
					default:
						AML_ERROR("FieldElement write_field (PCIConfig) with access size {}", access_size);
						return false;
				}
				return true;
			}
			default:
				AML_TODO("FieldElement write_field with region space {}", static_cast<uint8_t>(op_region->region_space));
				return false;
		}
	}

	BAN::RefPtr<AML::Node> AML::FieldElement::evaluate()
	{
		// Field LockRule only applies to modifying the field, not reading it

		auto access_type = determine_access_type();
		if (!access_type.has_value())
			return {};

		auto result = read_field(access_type->offset, access_type->access_size);
		if (!result.has_value())
			return {};

		return MUST(BAN::RefPtr<Integer>::create((result.value() >> access_type->shift) & access_type->mask));
	}

	bool AML::FieldElement::store(BAN::RefPtr<AML::Node> source)
	{
		auto source_integer = source->as_integer();
		if (!source_integer.has_value())
		{
			AML_TODO("FieldElement store with non-integer source, type {}", static_cast<uint8_t>(source->type));
			return false;
		}

		auto access_type = determine_access_type();
		if (!access_type.has_value())
			return false;

		if (access_rules.lock_rule == FieldRules::LockRule::Lock)
			Namespace::root_namespace()->global_lock.lock();
		BAN::ScopeGuard unlock_guard([&] {
			if (access_rules.lock_rule == FieldRules::LockRule::Lock)
				Namespace::root_namespace()->global_lock.unlock();
		});

		uint64_t to_write = 0;
		switch (access_rules.update_rule)
		{
			case FieldRules::UpdateRule::Preserve:
			{
				auto read_result = read_field(access_type->offset, access_type->access_size);
				if (!read_result.has_value())
					return false;
				to_write = read_result.value();
				to_write &= ~(access_type->mask << access_type->shift);
				to_write |= (source_integer.value() & access_type->mask) << access_type->shift;
				break;
			}
			case FieldRules::UpdateRule::WriteAsOnes:
				to_write = ~(access_type->mask << access_type->shift);
				to_write |= (source_integer.value() & access_type->mask) << access_type->shift;
				break;
			case FieldRules::UpdateRule::WriteAsZeros:
				to_write = 0;
				to_write |= (source_integer.value() & access_type->mask) << access_type->shift;
				break;
		}

		return write_field(access_type->offset, access_type->access_size, to_write);
	}

	void AML::FieldElement::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("FieldElement ");
		name.debug_print();
		AML_DEBUG_PRINT("({}, offset {}, OpRegion ", bit_count, bit_offset);
		op_region->name.debug_print();
		AML_DEBUG_PRINT(")");
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
		auto index_field_element = Namespace::root_namespace()->find_object(context.scope, index_field_element_name.value());
		if (!index_field_element || index_field_element->type != AML::Node::Type::FieldElement)
		{
			AML_ERROR("IndexField IndexName does not name a valid FieldElement");
			return ParseResult::Failure;
		}

		auto data_field_element_name = NameString::parse(field_pkg);
		if (!data_field_element_name.has_value())
			return ParseResult::Failure;
		auto data_field_element = Namespace::root_namespace()->find_object(context.scope, data_field_element_name.value());
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

	void AML::IndexFieldElement::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("IndexFieldElement ");
		name.debug_print();
		AML_DEBUG_PRINT("({}, offset {}, IndexName ", bit_count, bit_offset);
		index_element->name.debug_print();
		AML_DEBUG_PRINT(", DataName ");
		data_element->name.debug_print();
		AML_DEBUG_PRINT(")");
	}

}
