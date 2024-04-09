#pragma once

#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct OpRegion : public NamedObject
	{
		enum class RegionSpace
		{
			SystemMemory = 0,
			SystemIO = 1,
			PCIConfig = 2,
			EmbeddedController = 3,
			SMBus = 4,
			SystemCMOS = 5,
			PCIBarTarget = 6,
			IPMI = 7,
			GeneralPurposeIO = 8,
			GenericSerialBus = 9,
			PCC = 10,
		};
		RegionSpace space;
		uint64_t offset;
		uint64_t length;

		OpRegion(NameSeg name, RegionSpace space, uint64_t offset, uint64_t length)
			: NamedObject(Node::Type::OpRegion, name), space(space), offset(offset), length(length)
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
			if (!offset.has_value())
			{
				AML_ERROR("OpRegion offset must be an integer");
				return ParseResult::Failure;
			}

			auto length_result = AML::parse_object(context);
			if (!length_result.success())
				return ParseResult::Failure;
			auto length = length_result.node()->as_integer();
			if (!length.has_value())
			{
				AML_ERROR("OpRegion length must be an integer");
				return ParseResult::Failure;
			}

			auto op_region = MUST(BAN::RefPtr<OpRegion>::create(
				name->path.back(),
				region_space,
				offset.value(),
				length.value()
			));

			if (!context.root_namespace->add_named_object(context, name.value(), op_region))
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
			switch (space)
			{
				case RegionSpace::SystemMemory:			region_space_name = "SystemMemory"sv; break;
				case RegionSpace::SystemIO:				region_space_name = "SystemIO"sv; break;
				case RegionSpace::PCIConfig:			region_space_name = "PCIConfig"sv; break;
				case RegionSpace::EmbeddedController:	region_space_name = "EmbeddedController"sv; break;
				case RegionSpace::SMBus:				region_space_name = "SMBus"sv; break;
				case RegionSpace::SystemCMOS:			region_space_name = "SystemCMOS"sv; break;
				case RegionSpace::PCIBarTarget:			region_space_name = "PCIBarTarget"sv; break;
				case RegionSpace::IPMI:					region_space_name = "IPMI"sv; break;
				case RegionSpace::GeneralPurposeIO:		region_space_name = "GeneralPurposeIO"sv; break;
				case RegionSpace::GenericSerialBus:		region_space_name = "GenericSerialBus"sv; break;
				case RegionSpace::PCC:					region_space_name = "PCC"sv; break;
				default: region_space_name = "Unknown"sv; break;
			}
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("OperationRegion(");
			name.debug_print();
			AML_DEBUG_PRINT(", {}, 0x{H}, 0x{H})", region_space_name, offset, length);
		}

	};

}
