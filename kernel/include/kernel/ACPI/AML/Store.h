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
			auto source = source_result.node() ? source_result.node()->evaluate() : BAN::RefPtr<AML::Node>();
			if (!source)
			{
				AML_ERROR("Store source cannot be evaluated");
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

#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINTLN("Storing {");
			source->debug_print(1);
			AML_DEBUG_PRINTLN("");
			AML_DEBUG_PRINTLN("} to {");
			destination->debug_print(1);
			AML_DEBUG_PRINTLN("");
			AML_DEBUG_PRINTLN("}");
#endif

			if (!destination->store(source))
				return ParseResult::Failure;
			return ParseResult::Success;
		}
	};

}
