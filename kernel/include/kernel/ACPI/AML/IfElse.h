#pragma once

#include <BAN/HashMap.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>

namespace Kernel::ACPI::AML
{

	struct IfElse
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::IfOp);
			context.aml_data = context.aml_data.slice(1);

			auto if_pkg = AML::parse_pkg(context.aml_data);
			if (!if_pkg.has_value())
				return ParseResult::Failure;

			auto outer_aml_data = context.aml_data;
			context.aml_data = if_pkg.value();

			auto predicate_result = AML::parse_object(context);
			if (!predicate_result.success())
				return ParseResult::Failure;
			auto predicate_node = predicate_result.node() ? predicate_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
			if (!predicate_node)
			{
				AML_ERROR("If predicate is not an integer");
				return ParseResult::Failure;
			}
			const auto predicate_value = static_cast<AML::Integer*>(predicate_node.ptr())->value;

			// Else
			BAN::ConstByteSpan else_pkg;
			if (outer_aml_data.size() >= 1 && static_cast<AML::Byte>(outer_aml_data[0]) == Byte::ElseOp)
			{
				outer_aml_data = outer_aml_data.slice(1);
				auto else_pkg_result = AML::parse_pkg(outer_aml_data);
				if (!else_pkg_result.has_value())
					return ParseResult::Failure;
				else_pkg = else_pkg_result.value();
			}
			if (predicate_value == 0)
				context.aml_data = else_pkg;

			while (context.aml_data.size() > 0)
			{
				auto object_result = AML::parse_object(context);
				if (object_result.returned() || object_result.breaked() || object_result.continued())
					return object_result;
				if (!object_result.success())
					return ParseResult::Failure;
			}

			context.aml_data = outer_aml_data;

			return ParseResult::Success;
		}
	};

}
