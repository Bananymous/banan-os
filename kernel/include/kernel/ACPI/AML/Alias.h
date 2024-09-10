#pragma once

#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/Names.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct Alias
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::AliasOp);
			context.aml_data = context.aml_data.slice(1);

			auto source_string = AML::NameString::parse(context.aml_data);
			if (!source_string.has_value())
				return ParseResult::Failure;

			auto source_object = AML::Namespace::root_namespace()->find_object(context.scope, source_string.value(), AML::Namespace::FindMode::Normal);
			auto alias_string = AML::NameString::parse(context.aml_data);
			if (!alias_string.has_value())
				return ParseResult::Failure;

			if (!source_object)
			{
				AML_PRINT("Alias target could not be found");
				return ParseResult::Success;
			}

			if (!Namespace::root_namespace()->add_named_object(context, alias_string.value(), source_object))
				return ParseResult::Success;

	#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINT("Alias \"");
			alias_string->debug_print();
			AML_DEBUG_PRINT("\" => ");
			source_object->debug_print(0);
			AML_DEBUG_PRINTLN("");
	#endif

			return ParseResult::Success;
		}
	};

}
