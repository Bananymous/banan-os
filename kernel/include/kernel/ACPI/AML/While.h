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
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::WhileOp);
			context.aml_data = context.aml_data.slice(1);

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
				auto predicate = predicate_result.node() ? predicate_result.node()->as_integer() : BAN::Optional<uint64_t>();
				if (!predicate.has_value())
				{
					AML_ERROR("While predicate is not an integer");
					return ParseResult::Failure;
				}

				if (!predicate.value())
					break;

				while (context.aml_data.size() > 0)
				{
					// NOTE: we can just parse BreakOp here, since this is the only legal place for BreakOp
					if (static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::BreakOp)
					{
						context.aml_data = context.aml_data.slice(1);
						breaked = true;
						break;
					}
					auto object_result = AML::parse_object(context);
					if (object_result.returned())
						return ParseResult(ParseResult::Result::Returned, object_result.node());
					if (!object_result.success())
						return ParseResult::Failure;
				}
			}

			context.aml_data = outer_aml_data;

			return ParseResult::Success;
		}
	};

}
