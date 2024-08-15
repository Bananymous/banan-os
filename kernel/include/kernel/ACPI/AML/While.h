#pragma once

#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>

namespace Kernel::ACPI::AML
{

	struct While
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);

			AML::Byte opcode = static_cast<Byte>(context.aml_data[0]);
			context.aml_data = context.aml_data.slice(1);

			switch (opcode)
			{
				case AML::Byte::BreakOp:
					return ParseResult(ParseResult::Result::Breaked);
				case AML::Byte::ContinueOp:
					return ParseResult(ParseResult::Result::Continued);
				case AML::Byte::WhileOp:
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			auto while_pkg = AML::parse_pkg(context.aml_data);
			if (!while_pkg.has_value())
				return ParseResult::Failure;

			auto outer_aml_data = context.aml_data;

			bool breaked = false;
			while (!breaked)
			{
				context.aml_data = while_pkg.value();

				auto predicate_result = AML::parse_object(context);
				if (!predicate_result.success())
					return ParseResult::Failure;
				auto predicate_node = predicate_result.node()
					? predicate_result.node()->convert(AML::Node::ConvInteger)
					: BAN::RefPtr<AML::Node>();
				if (!predicate_node)
				{
					AML_ERROR("While predicate is not an integer");
					return ParseResult::Failure;
				}

				if (!static_cast<AML::Integer*>(predicate_node.ptr())->value)
					break;

				while (context.aml_data.size() > 0)
				{
					auto object_result = AML::parse_object(context);
					if (object_result.returned())
						return ParseResult(ParseResult::Result::Returned, object_result.node());
					if (object_result.breaked())
						breaked = true;
					if (object_result.breaked() || object_result.continued())
						break;
					if (!object_result.success())
						return ParseResult::Failure;
				}
			}

			context.aml_data = outer_aml_data;

			return ParseResult::Success;
		}
	};

}
