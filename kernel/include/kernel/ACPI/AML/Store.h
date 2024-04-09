#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct Store
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::StoreOp);
			context.aml_data = context.aml_data.slice(1);

			auto source_result = AML::parse_object(context);
			if (!source_result.success())
				return ParseResult::Failure;
			auto source = source_result.node();
			if (!source)
			{
				AML_ERROR("Store source is null");
				return ParseResult::Failure;
			}

			auto destination_result = AML::parse_object(context);
			if (!destination_result.success())
				return ParseResult::Failure;
			auto destination = destination_result.node();
			if (!destination)
			{
				AML_ERROR("Store destination is null");
				return ParseResult::Failure;
			}

			if (!destination->store(source))
				return ParseResult::Failure;
			return ParseResult::Success;
		}
	};

}
