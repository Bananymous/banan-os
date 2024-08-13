#pragma once

#include <BAN/Endianness.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	struct ThermalZone : public AML::Scope
	{
		ThermalZone(NameSeg name)
			: Scope(Node::Type::ThermalZone, name)
		{}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::ThermalZoneOp);
			context.aml_data = context.aml_data.slice(2);

			auto opt_thermal_zone_pkg = AML::parse_pkg(context.aml_data);
			if (!opt_thermal_zone_pkg.has_value())
				return ParseResult::Failure;
			auto thermal_zone_pkg = opt_thermal_zone_pkg.value();

			auto name = NameString::parse(thermal_zone_pkg);
			if (!name.has_value())
				return ParseResult::Failure;

			auto thermal_zone = MUST(BAN::RefPtr<ThermalZone>::create(name->path.back()));
			if (!Namespace::root_namespace()->add_named_object(context, name.value(), thermal_zone))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			thermal_zone->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return thermal_zone->enter_context_and_parse_term_list(context, name.value(), thermal_zone_pkg);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("ThermalZone {} {", name);
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
