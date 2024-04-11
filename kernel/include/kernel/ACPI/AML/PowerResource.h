#pragma once

#include <BAN/Endianness.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	struct PowerResource : public AML::Scope
	{
		uint8_t system_level;
		uint16_t resource_order;

		PowerResource(NameSeg name, uint8_t system_level, uint16_t resource_order)
			: Scope(Node::Type::PowerResource, name)
			, system_level(system_level)
			, resource_order(resource_order)
		{}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::PowerResOp);
			context.aml_data = context.aml_data.slice(2);

			auto opt_power_res_pkg = AML::parse_pkg(context.aml_data);
			if (!opt_power_res_pkg.has_value())
				return ParseResult::Failure;
			auto power_res_pkg = opt_power_res_pkg.value();

			auto name = NameString::parse(power_res_pkg);
			if (!name.has_value())
				return ParseResult::Failure;

			if (power_res_pkg.size() < 1)
				return ParseResult::Failure;
			uint8_t system_level = power_res_pkg[0];
			power_res_pkg = power_res_pkg.slice(1);

			if (power_res_pkg.size() < 2)
				return ParseResult::Failure;
			uint16_t resource_order = BAN::little_endian_to_host<uint16_t>(*reinterpret_cast<const uint16_t*>(power_res_pkg.data()));
			power_res_pkg = power_res_pkg.slice(2);

			auto power_res = MUST(BAN::RefPtr<PowerResource>::create(name->path.back(), system_level, resource_order));
			if (!Namespace::root_namespace()->add_named_object(context, name.value(), power_res))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			power_res->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return power_res->enter_context_and_parse_term_list(context, name.value(), power_res_pkg);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("PowerResource {} (SystemLevel {}, ResourceOrder {})", name, system_level, resource_order);
		}
	};

}
