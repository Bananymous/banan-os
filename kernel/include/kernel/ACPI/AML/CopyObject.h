#pragma once

#include <kernel/ACPI/AML/Alias.h>
#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Register.h>

namespace Kernel::ACPI::AML
{

	struct CopyObject
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::CopyObjectOp);
			context.aml_data = context.aml_data.slice(1);

			auto source_result = AML::parse_object(context);
			if (!source_result.success())
				return ParseResult::Failure;
			auto source = source_result.node()
				? source_result.node()->to_underlying()
				: BAN::RefPtr<AML::Node>();
			if (!source)
			{
				AML_ERROR("CopyObject source is null");
				return ParseResult::Failure;
			}

			auto destination_result = AML::parse_object(context);
			if (!destination_result.success())
				return ParseResult::Failure;
			auto destination = destination_result.node();
			if (!destination)
			{
				AML_ERROR("CopyObject destination is null");
				return ParseResult::Failure;
			}

#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINTLN("CopyObject {");
			source->debug_print(1);
			AML_DEBUG_PRINTLN("");
			AML_DEBUG_PRINTLN("} to {");
			destination->debug_print(1);
			AML_DEBUG_PRINTLN("");
			AML_DEBUG_PRINTLN("}");
#endif

			switch (destination->type)
			{
				case AML::Node::Type::Alias:
					static_cast<AML::Alias*>(destination.ptr())->target = source;
					return source;
				case AML::Node::Type::Name:
					static_cast<AML::Name*>(destination.ptr())->object = source;
					return source;
				case AML::Node::Type::Register:
					static_cast<AML::Register*>(destination.ptr())->value = source;
					return source;
				default:
					ASSERT_NOT_REACHED();
			}
		}
	};

}
