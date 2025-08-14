// FIXME: Find better ways to manage stack usage
#pragma GCC push_options
#pragma GCC optimize "no-inline"
#include <BAN/Errors.h>
#pragma GCC pop_options

#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/OpRegion.h>
#include <kernel/IO.h>
#include <kernel/Memory/PageTable.h>
#include <kernel/PCI.h>

namespace Kernel::ACPI::AML
{

	static BAN::ErrorOr<size_t> parse_pkg_length(BAN::ConstByteSpan& aml_data)
	{
		if (aml_data.empty())
			return BAN::Error::from_errno(ENODATA);

		const uint32_t encoding_length = (aml_data[0] >> 6) + 1;
		if (aml_data.size() < encoding_length)
			return BAN::Error::from_errno(ENODATA);

		uint32_t pkg_length = 0;
		switch (encoding_length)
		{
			case 1:
				pkg_length |= aml_data[0] & 0x3F;
				break;
			case 2:
				pkg_length |= aml_data[0] & 0x0F;
				pkg_length |= aml_data[1] << 4;
				break;
			case 3:
				pkg_length |= aml_data[0] & 0x0F;
				pkg_length |= aml_data[1] << 4;
				pkg_length |= aml_data[2] << 12;
				break;
			case 4:
				pkg_length |= aml_data[0] & 0x0F;
				pkg_length |= aml_data[1] << 4;
				pkg_length |= aml_data[2] << 12;
				pkg_length |= aml_data[3] << 20;
				break;
		}

		aml_data = aml_data.slice(encoding_length);
		return pkg_length;
	}

	BAN::ErrorOr<void> parse_opregion_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_opregion_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::OpRegionOp);
		context.aml_data = context.aml_data.slice(2);

		auto region_name = TRY(parse_name_string(context.aml_data));

		if (context.aml_data.empty())
			return BAN::Error::from_errno(ENODATA);
		auto region_space = context.aml_data[0];
		context.aml_data = context.aml_data.slice(1);

		switch (static_cast<GAS::AddressSpaceID>(region_space))
		{
			case GAS::AddressSpaceID::SystemMemory:
			case GAS::AddressSpaceID::SystemIO:
			case GAS::AddressSpaceID::PCIConfig:
			case GAS::AddressSpaceID::EmbeddedController:
			case GAS::AddressSpaceID::SMBus:
			case GAS::AddressSpaceID::SystemCMOS:
			case GAS::AddressSpaceID::PCIBarTarget:
			case GAS::AddressSpaceID::IPMI:
			case GAS::AddressSpaceID::GeneralPurposeIO:
			case GAS::AddressSpaceID::GenericSerialBus:
			case GAS::AddressSpaceID::PlatformCommunicationChannel:
				break;
			default:
				dprintln("OpRegion region space 0x{2H}?", region_space);
				break;
		}

		auto region_offset = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));
		auto region_length = TRY(convert_node(TRY(parse_node(context)), ConvInteger, sizeof(uint64_t)));

		Node opregion;
		opregion.type = Node::Type::OpRegion;
		opregion.as.opregion.address_space = static_cast<GAS::AddressSpaceID>(region_space);
		opregion.as.opregion.offset = region_offset.as.integer.value;
		opregion.as.opregion.length = region_length.as.integer.value;

		new (&opregion.as.opregion.scope()) Scope();
		opregion.as.opregion.scope() = TRY(context.scope.copy());

		TRY(Namespace::root_namespace().add_named_object(context, region_name, BAN::move(opregion)));

		return {};
	}

	template<typename F>
	static BAN::ErrorOr<void> parse_field_list(ParseContext& context, BAN::ConstByteSpan field_list_pkg, const F& create_element, uint8_t field_flags)
	{
		uint64_t offset = 0;
		while (!field_list_pkg.empty())
		{
			switch (field_list_pkg[0])
			{
				case 0x00:
					field_list_pkg = field_list_pkg.slice(1);
					offset += TRY(parse_pkg_length(field_list_pkg));
					break;
				case 0x01:
					// FIXME: do something with
					if (field_list_pkg.size() < 3)
						return BAN::Error::from_errno(ENODATA);
					field_flags &= 0xF0;
					field_flags |= field_list_pkg[1] & 0x0F;
					field_list_pkg = field_list_pkg.slice(3);
					break;
				case 0x02:
					dwarnln("TODO: connect field");
					return BAN::Error::from_errno(ENOTSUP);
				case 0x03:
					dwarnln("TODO: extended access field");
					return BAN::Error::from_errno(ENOTSUP);
				default:
				{
					if (field_list_pkg.size() < 4)
						return BAN::Error::from_errno(ENODATA);
					if (!is_lead_name_char(field_list_pkg[0]) || !is_name_char(field_list_pkg[1]) || !is_name_char(field_list_pkg[2]) || !is_name_char(field_list_pkg[3]))
					{
						dwarnln("Invalid NameSeg {2H}, {2H}, {2H}, {2H}",
							field_list_pkg[0], field_list_pkg[1], field_list_pkg[2], field_list_pkg[3]
						);
						return BAN::Error::from_errno(EINVAL);
					}
					const uint32_t name_seg = field_list_pkg.as<const uint32_t>();
					field_list_pkg = field_list_pkg.slice(4);
					const auto field_length = TRY(parse_pkg_length(field_list_pkg));

					NameString field_name;
					field_name.base = 0;
					TRY(field_name.parts.push_back(name_seg));

					Node field_node = create_element(offset, field_length, field_flags);

					TRY(Namespace::root_namespace().add_named_object(context, field_name, BAN::move(field_node)));

					offset += field_length;

					break;
				}
			}
		}

		return {};
	}

	BAN::ErrorOr<void> parse_field_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_field_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::FieldOp);
		context.aml_data = context.aml_data.slice(2);

		auto field_pkg = TRY(parse_pkg(context.aml_data));
		auto opregion_name = TRY(parse_name_string(field_pkg));
		if (field_pkg.empty())
			return BAN::Error::from_errno(ENODATA);
		auto default_flags = field_pkg[0];
		field_pkg = field_pkg.slice(1);

		auto [_, opregion] = TRY(Namespace::root_namespace().find_named_object(context.scope, opregion_name));
		if (opregion == nullptr)
		{
			dwarnln("could not find '{}'.'{}'", context.scope, opregion_name);
			return BAN::Error::from_errno(ENOENT);
		}
		if (opregion->node.type != Node::Type::OpRegion)
		{
			dwarnln("Field source is {}", opregion->node);
			return BAN::Error::from_errno(EINVAL);
		}

		const auto create_element =
			[&](uint64_t offset, uint64_t length, uint8_t field_flags) -> Node
			{
				Node field_node {};
				field_node.type = Node::Type::FieldUnit;
				field_node.as.field_unit.type = FieldUnit::Type::Field;
				field_node.as.field_unit.as.field.opregion = opregion->node.as.opregion;
				field_node.as.field_unit.length = length;
				field_node.as.field_unit.offset = offset;
				field_node.as.field_unit.flags = field_flags;
				return field_node;
			};

		TRY(parse_field_list(context, field_pkg, create_element, default_flags));

		return {};
	}

	BAN::ErrorOr<void> parse_index_field_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_index_field_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::IndexFieldOp);
		context.aml_data = context.aml_data.slice(2);

		auto field_pkg = TRY(parse_pkg(context.aml_data));

		auto index_name = TRY(parse_name_string(field_pkg));
		auto data_name = TRY(parse_name_string(field_pkg));
		if (field_pkg.empty())
			return BAN::Error::from_errno(ENODATA);
		auto default_flags = field_pkg[0];
		field_pkg = field_pkg.slice(1);

		auto [_1, index_obj] = TRY(Namespace::root_namespace().find_named_object(context.scope, index_name));
		if (index_obj == nullptr)
		{
			dwarnln("could not find '{}'.'{}'", context.scope, index_name);
			return BAN::Error::from_errno(ENOENT);
		}
		if (index_obj->node.type != Node::Type::FieldUnit)
		{
			dwarnln("IndexField source is {}", index_obj->node);
			return BAN::Error::from_errno(EINVAL);
		}

		auto [_2, data_obj] = TRY(Namespace::root_namespace().find_named_object(context.scope, data_name));
		if (data_obj == nullptr)
		{
			dwarnln("could not find '{}'.'{}'", context.scope, data_name);
			return BAN::Error::from_errno(ENOENT);
		}
		if (data_obj->node.type != Node::Type::FieldUnit)
		{
			dwarnln("IndexField source is {}", data_obj->node);
			return BAN::Error::from_errno(EINVAL);
		}

		const auto create_element =
			[&](uint64_t offset, uint64_t length, uint8_t field_flags) -> Node
			{
				Node field_node {};
				field_node.type = Node::Type::FieldUnit;
				field_node.as.field_unit.type = FieldUnit::Type::IndexField;
				field_node.as.field_unit.as.index_field.index = index_obj;
				field_node.as.field_unit.as.index_field.index->ref_count++;
				field_node.as.field_unit.as.index_field.data = data_obj;
				field_node.as.field_unit.as.index_field.data->ref_count++;
				field_node.as.field_unit.length = length;
				field_node.as.field_unit.offset = offset;
				field_node.as.field_unit.flags = field_flags;
				return field_node;
			};

		TRY(parse_field_list(context, field_pkg, create_element, default_flags));

		return {};
	}

	BAN::ErrorOr<void> parse_bank_field_op(ParseContext& context)
	{
		dprintln_if(AML_DUMP_FUNCTION_CALLS, "parse_bank_field_op");

		ASSERT(context.aml_data.size() >= 2);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
		ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::BankFieldOp);
		context.aml_data = context.aml_data.slice(2);

		auto field_pkg = TRY(parse_pkg(context.aml_data));
		auto opregion_name = TRY(parse_name_string(field_pkg));
		auto bank_selector_name = TRY(parse_name_string(field_pkg));

		auto temp_aml_data = context.aml_data;
		context.aml_data = field_pkg;
		auto bank_value_node = TRY(parse_node(context));
		field_pkg = context.aml_data;
		context.aml_data = temp_aml_data;

		if (field_pkg.empty())
			return BAN::Error::from_errno(ENODATA);
		auto default_flags = field_pkg[0];
		field_pkg = field_pkg.slice(1);

		auto [_1, opregion] = TRY(Namespace::root_namespace().find_named_object(context.scope, opregion_name));
		if (opregion == nullptr)
		{
			dwarnln("could not find '{}'.'{}'", context.scope, opregion_name);
			return BAN::Error::from_errno(ENOENT);
		}
		if (opregion->node.type != Node::Type::OpRegion)
		{
			dwarnln("Field source is {}", opregion->node);
			return BAN::Error::from_errno(EINVAL);
		}

		auto [_2, bank_selector] = TRY(Namespace::root_namespace().find_named_object(context.scope, bank_selector_name));
		if (bank_selector == nullptr)
		{
			dwarnln("could not find '{}'.'{}'", context.scope, bank_selector_name);
			return BAN::Error::from_errno(ENOENT);
		}
		if (bank_selector->node.type != Node::Type::FieldUnit)
		{
			dwarnln("BankField bank selector is {}", bank_selector->node);
			return BAN::Error::from_errno(EINVAL);
		}

		const uint64_t bank_value = TRY(convert_node(BAN::move(bank_value_node), ConvInteger, sizeof(uint64_t))).as.integer.value;

		const auto create_element =
			[&](uint64_t offset, uint64_t length, uint8_t field_flags) -> Node
			{
				Node field_node {};
				field_node.type = Node::Type::FieldUnit;
				field_node.as.field_unit.type = FieldUnit::Type::BankField;
				field_node.as.field_unit.as.bank_field.opregion = opregion->node.as.opregion;
				field_node.as.field_unit.as.bank_field.bank_selector = bank_selector;
				field_node.as.field_unit.as.bank_field.bank_selector->ref_count++;
				field_node.as.field_unit.as.bank_field.bank_value = bank_value;
				field_node.as.field_unit.length = length;
				field_node.as.field_unit.offset = offset;
				field_node.as.field_unit.flags = field_flags;
				return field_node;
			};

		TRY(parse_field_list(context, field_pkg, create_element, default_flags));

		return {};
	}

	struct AccessRule
	{
		uint8_t access_bits;
		bool lock;
		enum {
			Preserve,
			WriteZeros,
			WriteOnes,
		} update_rule;
	};

	static AccessRule parse_access_rule(uint8_t flags)
	{
		AccessRule rule;

		switch (flags & 0x0F)
		{
			case 0: rule.access_bits =  8; break;
			case 1: rule.access_bits =  8; break;
			case 2: rule.access_bits = 16; break;
			case 3: rule.access_bits = 32; break;
			case 4: rule.access_bits = 64; break;
			case 5: rule.access_bits =  8; break;
			default: ASSERT_NOT_REACHED();
		}

		rule.lock = !!(flags & 0x10);

		switch ((flags >> 5) & 0x03)
		{
			case 0: rule.update_rule = AccessRule::Preserve;   break;
			case 1: rule.update_rule = AccessRule::WriteOnes;  break;
			case 2: rule.update_rule = AccessRule::WriteZeros; break;
			default: ASSERT_NOT_REACHED();
		}

		return rule;
	}

	static BAN::ErrorOr<void> get_pci_config_address(const OpRegion& opregion, uint16_t& seg, uint8_t& bus, uint8_t& dev, uint8_t& func)
	{
		ASSERT(opregion.address_space == GAS::AddressSpaceID::PCIConfig);

		seg = 0;
		if (auto seg_res = TRY(Namespace::root_namespace().find_named_object(opregion.scope(), TRY(AML::NameString::from_string("_SEG"_sv)), true)); seg_res.node != nullptr)
			seg = TRY(convert_node(TRY(evaluate_node(seg_res.path, seg_res.node->node)), ConvInteger, -1)).as.integer.value;

		bus = 0;
		if (auto bbn_res = TRY(Namespace::root_namespace().find_named_object(opregion.scope(), TRY(AML::NameString::from_string("_BBN"_sv)), true)); bbn_res.node != nullptr)
			bus = TRY(convert_node(TRY(evaluate_node(bbn_res.path, bbn_res.node->node)), ConvInteger, -1)).as.integer.value;

		auto adr_res = TRY(Namespace::root_namespace().find_named_object(opregion.scope(), TRY(AML::NameString::from_string("_ADR"_sv)), true));
		if (adr_res.node == nullptr)
		{
			dwarnln("No _ADR for PCIConfig OpRegion");
			return BAN::Error::from_errno(EFAULT);
		}

		auto adr_node = TRY(convert_node(TRY(evaluate_node(adr_res.path, adr_res.node->node)), ConvInteger, -1));
		dev  = adr_node.as.integer.value >> 16;
		func = adr_node.as.integer.value & 0xFF;

		return {};
	}

	static BAN::ErrorOr<EmbeddedController*> get_embedded_controller(const OpRegion& opregion)
	{
		ASSERT(opregion.address_space == GAS::AddressSpaceID::EmbeddedController);

		auto all_embedded_controllers = ACPI::get().embedded_controllers();
		for (auto& embedded_controller : all_embedded_controllers)
			if (embedded_controller->scope() == opregion.scope())
				return embedded_controller.ptr();

		return BAN::Error::from_errno(ENOENT);
	}

	static BAN::ErrorOr<uint64_t> perform_opregion_read(const OpRegion& opregion, uint8_t access_size, uint64_t offset)
	{
		ASSERT(offset % access_size == 0);

		const uint64_t byte_offset = opregion.offset + offset;

		switch (opregion.address_space)
		{
			case GAS::AddressSpaceID::SystemMemory:
			{
				uint64_t result;
				PageTable::with_fast_page(byte_offset & PAGE_ADDR_MASK, [&]() {
					void* addr = PageTable::fast_page_as_ptr(byte_offset % PAGE_SIZE);
					switch (access_size) {
						case 1: result = *static_cast<uint8_t* >(addr); break;
						case 2: result = *static_cast<uint16_t*>(addr); break;
						case 4: result = *static_cast<uint32_t*>(addr); break;
						case 8: result = *static_cast<uint64_t*>(addr); break;
						default: ASSERT_NOT_REACHED();
					}
				});
				return result;
			}
			case GAS::AddressSpaceID::SystemIO:
				if (byte_offset + access_size > 0x10000)
				{
					dwarnln("{} byte read from IO port 0x{H}", access_size, byte_offset);
					return BAN::Error::from_errno(EINVAL);
				}
				switch (access_size)
				{
					case 1: return IO::inb(byte_offset);
					case 2: return IO::inw(byte_offset);
					case 4: return IO::inl(byte_offset);
					default:
						dwarnln("{} byte read from IO port", access_size);
						return BAN::Error::from_errno(EINVAL);
				}
				ASSERT_NOT_REACHED();
			case GAS::AddressSpaceID::PCIConfig:
			{
				uint16_t seg;
				uint8_t bus, dev, func;
				TRY(get_pci_config_address(opregion, seg, bus, dev, func));

				if (seg != 0)
				{
					dwarnln("PCIConfig OpRegion with segment");
					return BAN::Error::from_errno(ENOTSUP);
				}

				switch (access_size)
				{
					case 1: return PCI::PCIManager::get().read_config_byte (bus, dev, func, byte_offset);
					case 2: return PCI::PCIManager::get().read_config_word (bus, dev, func, byte_offset);
					case 4: return PCI::PCIManager::get().read_config_dword(bus, dev, func, byte_offset);
					default:
						dwarnln("{} byte read from PCI {2H}:{2H}:{2H} offset {2H}", access_size, bus, dev, func, byte_offset);
						return BAN::Error::from_errno(EINVAL);
				}
				ASSERT_NOT_REACHED();
			}
			case GAS::AddressSpaceID::EmbeddedController:
			{
				auto* embedded_controller = TRY(get_embedded_controller(opregion));
				ASSERT(embedded_controller);

				if (access_size != 1)
				{
					dwarnln("{} byte read from embedded controller", access_size);
					return BAN::Error::from_errno(EINVAL);
				}

				return TRY(embedded_controller->read_byte(offset));
			}
			case GAS::AddressSpaceID::SMBus:
			case GAS::AddressSpaceID::SystemCMOS:
			case GAS::AddressSpaceID::PCIBarTarget:
			case GAS::AddressSpaceID::IPMI:
			case GAS::AddressSpaceID::GeneralPurposeIO:
			case GAS::AddressSpaceID::GenericSerialBus:
			case GAS::AddressSpaceID::PlatformCommunicationChannel:
				dwarnln("TODO: Read from address space 0x{2H}", static_cast<uint8_t>(opregion.address_space));
				return BAN::Error::from_errno(ENOTSUP);
		}

		ASSERT_NOT_REACHED();
	}

	static BAN::ErrorOr<void> perform_opregion_write(const OpRegion& opregion, uint8_t access_size, uint64_t offset, uint64_t value)
	{
		ASSERT(offset % access_size == 0);

		const uint64_t byte_offset = opregion.offset + offset;

		switch (opregion.address_space)
		{
			case GAS::AddressSpaceID::SystemMemory:
				PageTable::with_fast_page(byte_offset & PAGE_ADDR_MASK, [&]() {
					void* addr = PageTable::fast_page_as_ptr(byte_offset % PAGE_SIZE);
					switch (access_size) {
						case 1: *static_cast<uint8_t* >(addr) = value; break;
						case 2: *static_cast<uint16_t*>(addr) = value; break;
						case 4: *static_cast<uint32_t*>(addr) = value; break;
						case 8: *static_cast<uint64_t*>(addr) = value; break;
						default: ASSERT_NOT_REACHED();
					}
				});
				return {};
			case GAS::AddressSpaceID::SystemIO:
				if (byte_offset + access_size > 0x10000)
				{
					dwarnln("{} byte write to IO port 0x{H}", access_size, byte_offset);
					return BAN::Error::from_errno(EINVAL);
				}
				switch (access_size)
				{
					case 1: IO::outb(byte_offset, value); break;
					case 2: IO::outw(byte_offset, value); break;
					case 4: IO::outl(byte_offset, value); break;
					default:
						dwarnln("{} byte write to IO port", access_size);
						return BAN::Error::from_errno(EINVAL);
				}
				return {};
			case GAS::AddressSpaceID::PCIConfig:
			{
				uint16_t seg;
				uint8_t bus, dev, func;
				TRY(get_pci_config_address(opregion, seg, bus, dev, func));

				if (seg != 0)
				{
					dwarnln("PCIConfig OpRegion with segment");
					return BAN::Error::from_errno(ENOTSUP);
				}

				switch (access_size)
				{
					case 1: PCI::PCIManager::get().write_config_byte (bus, dev, func, byte_offset, value); break;
					case 2: PCI::PCIManager::get().write_config_word (bus, dev, func, byte_offset, value); break;
					case 4: PCI::PCIManager::get().write_config_dword(bus, dev, func, byte_offset, value); break;
					default:
						dwarnln("{} byte write to PCI {2H}:{2H}:{2H} offset {2H}", access_size, bus, dev, func, byte_offset);
						return BAN::Error::from_errno(EINVAL);
				}
				return {};
			}
			case GAS::AddressSpaceID::EmbeddedController:
			{
				auto* embedded_controller = TRY(get_embedded_controller(opregion));
				ASSERT(embedded_controller);

				if (access_size != 1)
				{
					dwarnln("{} byte write to embedded controller", access_size);
					return BAN::Error::from_errno(EINVAL);
				}

				TRY(embedded_controller->write_byte(offset, value));
				return {};
			}
			case GAS::AddressSpaceID::SMBus:
			case GAS::AddressSpaceID::SystemCMOS:
			case GAS::AddressSpaceID::PCIBarTarget:
			case GAS::AddressSpaceID::IPMI:
			case GAS::AddressSpaceID::GeneralPurposeIO:
			case GAS::AddressSpaceID::GenericSerialBus:
			case GAS::AddressSpaceID::PlatformCommunicationChannel:
				dwarnln("TODO: Write to address space 0x{2H}", static_cast<uint8_t>(opregion.address_space));
				return BAN::Error::from_errno(ENOTSUP);
		}

		ASSERT_NOT_REACHED();
	}

	struct BufferInfo
	{
		const uint8_t* buffer;
		const uint64_t bytes;
	};

	static BAN::ErrorOr<BufferInfo> extract_buffer_info(const Node& source)
	{
		switch (source.type)
		{
			case Node::Type::String:
			case Node::Type::Buffer:
				return BufferInfo {
					.buffer = source.as.str_buf->bytes,
					.bytes = source.as.str_buf->size,
				};
			case Node::Type::Integer:
				return BufferInfo {
					.buffer = reinterpret_cast<const uint8_t*>(&source.as.integer.value),
					.bytes = sizeof(uint64_t),
				};
			default:
				dwarnln("Invalid store of {} to FieldUnit", source);
				return BAN::Error::from_errno(EINVAL);
		}

		ASSERT_NOT_REACHED();
	}

	static BAN::ErrorOr<Node> allocate_destination(const Node& source, Node::Type type, size_t max_bytes)
	{
		ASSERT(source.type == Node::Type::FieldUnit);

		const uint64_t source_bit_length = source.as.field_unit.length;

		Node result;
		result.type = type;

		switch (type)
		{
			case Node::Type::Buffer:
			{
				const size_t dst_bits = BAN::Math::max<size_t>(max_bytes * 8, source_bit_length);
				const size_t bytes = BAN::Math::div_round_up<size_t>(dst_bits, 8);

				result.as.str_buf = static_cast<Buffer*>(kmalloc(sizeof(Buffer) + bytes));
				if (result.as.str_buf == nullptr)
					return BAN::Error::from_errno(ENOMEM);
				result.as.str_buf->size = bytes;
				result.as.str_buf->ref_count = 1;
				memset(result.as.str_buf->bytes, 0, bytes);

				break;
			}
			case Node::Type::Integer:
				if (source_bit_length > 64)
				{
					dwarnln("Convert field unit of {} bits to an integer", source_bit_length);
					return BAN::Error::from_errno(EINVAL);
				}

				result.as.integer.value = 0;

				break;
			default:
				ASSERT_NOT_REACHED();
		}

		return result;
	}

	static void write_bits_to_buffer(uint8_t* buffer, size_t bit_offset, uint64_t value, size_t bit_count)
	{
		size_t bits_done = 0;
		while (bits_done < bit_count) {
			const size_t acc_bit_offset = (bit_offset + bits_done) % 8;
			const size_t acc_size       = BAN::Math::min<size_t>(bit_count - bits_done, 8 - acc_bit_offset);
			const size_t mask           = (1 << acc_size) - 1;

			buffer[(bit_offset + bits_done) / 8] |= ((value >> bits_done) & mask) << acc_bit_offset;

			bits_done += acc_size;
		}
	}

	static uint64_t read_bits_from_buffer(const uint8_t* buffer, size_t bit_offset, size_t bit_count)
	{
		uint64_t result = 0;

		size_t bits_done = 0;
		while (bits_done < bit_count) {
			const size_t acc_bit_offset = (bit_offset + bits_done) % 8;
			const size_t acc_size       = BAN::Math::min<size_t>(bit_count - bits_done, 8 - acc_bit_offset);
			const size_t mask           = (1 << acc_size) - 1;

			result |= static_cast<uint64_t>((buffer[(bit_offset + bits_done) / 8] >> acc_bit_offset) & mask) << bits_done;

			bits_done += acc_size;
		}

		return result;
	}

	static BAN::ErrorOr<uint64_t> perform_index_field_read(const Node& source, uint64_t acc_byte_offset)
	{
		ASSERT(source.type == Node::Type::FieldUnit);
		ASSERT(source.as.field_unit.type == FieldUnit::Type::IndexField);

		Node index_node;
		index_node.type = Node::Type::Integer;
		index_node.as.integer.value = acc_byte_offset;
		TRY(store_to_field_unit(index_node, source.as.field_unit.as.index_field.index->node));

		auto value = TRY(convert_from_field_unit(source.as.field_unit.as.index_field.data->node, ConvInteger, sizeof(uint64_t)));
		return value.as.integer.value;
	}

	static BAN::ErrorOr<void> perform_index_field_write(const Node& source, uint64_t acc_byte_offset, uint64_t value)
	{
		ASSERT(source.type == Node::Type::FieldUnit);
		ASSERT(source.as.field_unit.type == FieldUnit::Type::IndexField);

		Node index_node;
		index_node.type = Node::Type::Integer;
		index_node.as.integer.value = acc_byte_offset;
		TRY(store_to_field_unit(index_node, source.as.field_unit.as.index_field.index->node));

		Node data_node;
		data_node.type = Node::Type::Integer;
		data_node.as.integer.value = value;
		TRY(store_to_field_unit(data_node, source.as.field_unit.as.index_field.data->node));
		return {};
	}

	BAN::ErrorOr<Node> convert_from_field_unit(const Node& source, uint8_t conversion, uint64_t max_length)
	{
		ASSERT(source.type == Node::Type::FieldUnit);

		const bool can_be_integer = source.as.field_unit.length <= 64;

		auto target_type = Node::Type::Uninitialized;
		if (can_be_integer && (conversion & Conversion::ConvInteger))
			target_type = Node::Type::Integer;
		else if (conversion & Conversion::ConvBuffer)
			target_type = Node::Type::Buffer;

		if (target_type == Node::Type::Uninitialized)
		{
			dwarnln("Invalid conversion from field unit to 0b{3b}", conversion);
			return BAN::Error::from_errno(EINVAL);
		}

		if (source.as.field_unit.type == FieldUnit::Type::BankField)
		{
			Node bank_node;
			bank_node.type = Node::Type::Integer;
			bank_node.as.integer.value = source.as.field_unit.as.bank_field.bank_value;
			TRY(store_to_field_unit(bank_node, source.as.field_unit.as.bank_field.bank_selector->node));
		}

		Node result = TRY(allocate_destination(source, target_type, max_length));
		const auto [dst_buf, dst_bytes] = TRY(extract_buffer_info(result));
		const auto [max_acc_bits, lock, _] = parse_access_rule(source.as.field_unit.flags);

		const size_t transfer_bits = BAN::Math::min<size_t>(source.as.field_unit.length, dst_bytes * 8);

		size_t bits_done = 0;
		while (bits_done < transfer_bits)
		{
			const size_t acc_bit_offset  = (source.as.field_unit.offset + bits_done) & (max_acc_bits - 1);
			const size_t acc_bit_count   = max_acc_bits - acc_bit_offset;
			const size_t acc_byte_offset = ((source.as.field_unit.offset + bits_done) & ~(max_acc_bits - 1)) / 8;

			uint64_t value = 0;
			switch (source.as.field_unit.type)
			{
				case FieldUnit::Type::Field:
					value = TRY(perform_opregion_read(source.as.field_unit.as.field.opregion, max_acc_bits / 8, acc_byte_offset));
					break;
				case FieldUnit::Type::IndexField:
					value = TRY(perform_index_field_read(source, acc_byte_offset));
					break;
				case FieldUnit::Type::BankField:
					value = TRY(perform_opregion_read(source.as.field_unit.as.bank_field.opregion, max_acc_bits / 8, acc_byte_offset));
					break;
			}

			write_bits_to_buffer(const_cast<uint8_t*>(dst_buf), bits_done, value >> acc_bit_offset, acc_bit_count);

			bits_done += acc_bit_count;
		}

		return result;
	}

	BAN::ErrorOr<void> store_to_field_unit(const Node& source, const Node& target)
	{
		ASSERT(target.type == Node::Type::FieldUnit);

		switch (source.type)
		{
			case Node::Type::Integer:
			case Node::Type::Buffer:
			case Node::Type::String:
				break;
			default:
				return store_to_field_unit(
					TRY(convert_node(TRY(source.copy()), ConvInteger | ConvBuffer | ConvString, 0xFFFFFFFFFFFFFFFF)),
					target
				);
		}

		if (target.as.field_unit.type == FieldUnit::Type::BankField)
		{
			Node bank_node;
			bank_node.type = Node::Type::Integer;
			bank_node.as.integer.value = target.as.field_unit.as.bank_field.bank_value;
			TRY(store_to_field_unit(bank_node, target.as.field_unit.as.bank_field.bank_selector->node));
		}

		const auto [src_buf, src_bytes] = TRY(extract_buffer_info(source));
		const auto [max_acc_bits, lock, update_rule] = parse_access_rule(target.as.field_unit.flags);

		const size_t transfer_bits = BAN::Math::min<size_t>(target.as.field_unit.length, src_bytes * 8);

		size_t bits_done = 0;
		while (bits_done < transfer_bits)
		{
			const size_t acc_bit_offset  = (target.as.field_unit.offset + bits_done) & (max_acc_bits - 1);
			const size_t acc_bit_count   = BAN::Math::min<size_t>(max_acc_bits - acc_bit_offset, transfer_bits - bits_done);
			const size_t acc_byte_offset = ((target.as.field_unit.offset + bits_done) & ~(max_acc_bits - 1)) / 8;

			uint64_t value;
			switch (update_rule)
			{
				case AccessRule::Preserve:
					switch (target.as.field_unit.type)
					{
						case FieldUnit::Type::Field:
							value = TRY(perform_opregion_read(target.as.field_unit.as.field.opregion, max_acc_bits / 8, acc_byte_offset));
							break;
						case FieldUnit::Type::IndexField:
							value = TRY(perform_index_field_read(target, acc_byte_offset));
							break;
						case FieldUnit::Type::BankField:
							value = TRY(perform_opregion_read(target.as.field_unit.as.bank_field.opregion, max_acc_bits / 8, acc_byte_offset));
							break;
						default:
							ASSERT_NOT_REACHED();
					}
					break;
				case AccessRule::WriteZeros:
					value = 0x0000000000000000;
					break;
				case AccessRule::WriteOnes:
					value = 0xFFFFFFFFFFFFFFFF;
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			value &= ~(((static_cast<uint64_t>(1) << acc_bit_count) - 1) << acc_bit_offset);
			value |= read_bits_from_buffer(src_buf, bits_done, acc_bit_count) << acc_bit_offset;

			switch (target.as.field_unit.type)
			{
				case FieldUnit::Type::Field:
					TRY(perform_opregion_write(target.as.field_unit.as.field.opregion, max_acc_bits / 8, acc_byte_offset, value));
					break;
				case FieldUnit::Type::IndexField:
					TRY(perform_index_field_write(target, acc_byte_offset, value));
					break;
				case FieldUnit::Type::BankField:
					TRY(perform_opregion_write(target.as.field_unit.as.bank_field.opregion, max_acc_bits / 8, acc_byte_offset, value));
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			bits_done += acc_bit_count;
		}

		return {};
	}

}
