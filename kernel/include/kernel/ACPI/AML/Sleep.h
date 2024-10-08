#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/Timer/Timer.h>

namespace Kernel::ACPI::AML
{

	struct Sleep
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::SleepOp);
			context.aml_data = context.aml_data.slice(2);

			auto sleep_time_result = AML::parse_object(context);
			if (!sleep_time_result.success())
				return ParseResult::Failure;
			auto sleep_time_node = sleep_time_result.node()
				? sleep_time_result.node()->convert(AML::Node::ConvInteger)
				: BAN::RefPtr<AML::Node>();
			if (!sleep_time_node)
			{
				AML_ERROR("Sleep time cannot be evaluated to an integer");
				return ParseResult::Failure;
			}

			const auto sleep_time_value = static_cast<AML::Integer*>(sleep_time_node.ptr())->value;

#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINTLN("Sleeping for {} ms", sleep_time_value);
#endif

			SystemTimer::get().sleep_ms(sleep_time_value);
			return ParseResult::Success;
		}
	};

}
