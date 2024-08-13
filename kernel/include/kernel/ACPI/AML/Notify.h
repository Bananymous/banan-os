#pragma once

#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Processor.h>
#include <kernel/ACPI/AML/ThermalZone.h>

namespace Kernel::ACPI::AML
{

	struct Notify
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::NotifyOp);
			context.aml_data = context.aml_data.slice(1);

			auto object_result = AML::parse_object(context);
			if (!object_result.success())
				return ParseResult::Failure;
			auto object = object_result.node();
			if (!object)
			{
				AML_ERROR("Notify object is null");
				return ParseResult::Failure;
			}

			auto value_result = AML::parse_object(context);
			if (!value_result.success())
				return ParseResult::Failure;
			auto value = value_result.node() ? value_result.node()->as_integer() : BAN::RefPtr<AML::Integer>();
			if (!value)
			{
				AML_ERROR("Notify value is not an integer");
				return ParseResult::Failure;
			}

			BAN::StringView object_type_sv;
			BAN::StringView object_name_sv;
			switch (object->type)
			{
				case AML::Node::Type::Device:
					object_type_sv = "Device"_sv;
					object_name_sv = static_cast<AML::Device*>(object.ptr())->name.sv();
					break;
				case AML::Node::Type::Processor:
					object_type_sv = "Processor"_sv;
					object_name_sv = static_cast<AML::Processor*>(object.ptr())->name.sv();
					break;
				case AML::Node::Type::ThermalZone:
					object_type_sv = "ThermalZone"_sv;
					object_name_sv = static_cast<AML::ThermalZone*>(object.ptr())->name.sv();
					break;
				default:
					object_type_sv = "Unknown"_sv;
					object_name_sv = "????"_sv;
					break;
			}

			AML_TODO("Notify: {} {}: {2H}", object_type_sv, object_name_sv, value->value);
			return ParseResult::Success;
		}
	};

}
