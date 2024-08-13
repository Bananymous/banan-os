#pragma once

#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct OpRegion : public NamedObject
	{
		using RegionSpace = GAS::AddressSpaceID;
		RegionSpace region_space;
		uint64_t region_offset;
		uint64_t region_length;

		Kernel::Mutex mutex;

		OpRegion(NameSeg name, RegionSpace region_space, uint64_t region_offset, uint64_t region_length)
			: NamedObject(Node::Type::OpRegion, name)
			, region_space(region_space)
			, region_offset(region_offset)
			, region_length(region_length)
		{}

		static ParseResult parse(AML::ParseContext& context)
		{
			ASSERT(context.aml_data.size() > 2);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::ExtOpPrefix);
			ASSERT(static_cast<ExtOp>(context.aml_data[1]) == ExtOp::OpRegionOp);
			context.aml_data = context.aml_data.slice(2);

			auto name = NameString::parse(context.aml_data);
			if (!name.has_value())
				return ParseResult::Failure;

			if (context.aml_data.size() < 1)
				return ParseResult::Failure;
			auto region_space = static_cast<RegionSpace>(context.aml_data[0]);
			context.aml_data = context.aml_data.slice(1);

			auto offset_result = AML::parse_object(context);
			if (!offset_result.success())
				return ParseResult::Failure;
			auto offset = offset_result.node()->as_integer();
			if (!offset)
			{
				AML_ERROR("OpRegion offset must be an integer");
				return ParseResult::Failure;
			}

			auto length_result = AML::parse_object(context);
			if (!length_result.success())
				return ParseResult::Failure;
			auto length = length_result.node()->as_integer();
			if (!length)
			{
				AML_ERROR("OpRegion length must be an integer");
				return ParseResult::Failure;
			}

			auto op_region = MUST(BAN::RefPtr<OpRegion>::create(
				name->path.back(),
				region_space,
				offset->value,
				length->value
			));

			if (!Namespace::root_namespace()->add_named_object(context, name.value(), op_region))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			op_region->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult::Success;
		}

		virtual void debug_print(int indent) const override
		{
			BAN::StringView region_space_name;
			switch (region_space)
			{
				case RegionSpace::SystemMemory:					region_space_name = "SystemMemory"_sv; break;
				case RegionSpace::SystemIO:						region_space_name = "SystemIO"_sv; break;
				case RegionSpace::PCIConfig:					region_space_name = "PCIConfig"_sv; break;
				case RegionSpace::EmbeddedController:			region_space_name = "EmbeddedController"_sv; break;
				case RegionSpace::SMBus:						region_space_name = "SMBus"_sv; break;
				case RegionSpace::SystemCMOS:					region_space_name = "SystemCMOS"_sv; break;
				case RegionSpace::PCIBarTarget:					region_space_name = "PCIBarTarget"_sv; break;
				case RegionSpace::IPMI:							region_space_name = "IPMI"_sv; break;
				case RegionSpace::GeneralPurposeIO:				region_space_name = "GeneralPurposeIO"_sv; break;
				case RegionSpace::GenericSerialBus:				region_space_name = "GenericSerialBus"_sv; break;
				case RegionSpace::PlatformCommunicationChannel:	region_space_name = "PlatformCommunicationChannel"_sv; break;
				default: region_space_name = "Unknown"_sv; break;
			}
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("OperationRegion(");
			name.debug_print();
			AML_DEBUG_PRINT(", {}, 0x{H}, 0x{H})", region_space_name, region_offset, region_length);
		}

	};

}
