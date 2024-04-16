#pragma once

#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	struct Device : public AML::Scope
	{
		Device(NameSeg name)
			: Scope(Node::Type::Device, name)
		{}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::DeviceOp);
			context.aml_data = context.aml_data.slice(2);

			auto device_pkg = AML::parse_pkg(context.aml_data);
			if (!device_pkg.has_value())
				return ParseResult::Failure;

			auto name_string = AML::NameString::parse(device_pkg.value());
			if (!name_string.has_value())
				return ParseResult::Failure;

			auto device = MUST(BAN::RefPtr<Device>::create(name_string->path.back()));
			if (!Namespace::root_namespace()->add_named_object(context, name_string.value(), device))
				return ParseResult::Failure;

			return device->enter_context_and_parse_term_list(context, name_string.value(), device_pkg.value());
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Device ");
			name.debug_print();
			AML_DEBUG_PRINTLN(" {");
			Namespace::root_namespace()->for_each_child(scope,
				[&](const auto&, const auto& child)
				{
					child->debug_print(indent + 1);
					AML_DEBUG_PRINTLN("");
				}
			);
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
