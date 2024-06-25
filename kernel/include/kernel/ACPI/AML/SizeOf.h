#pragma once


#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Names.h>
#include <kernel/ACPI/AML/Package.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Reference.h>

namespace Kernel::ACPI::AML
{

	struct SizeOf
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::SizeOfOp);
			context.aml_data = context.aml_data.slice(1);

			auto object_result = AML::parse_object(context);
			if (!object_result.success())
				return ParseResult::Failure;
			auto object = object_result.node()->evaluate();
			if (object && object->type == AML::Node::Type::Reference)
				object = static_cast<AML::Reference*>(object.ptr())->node->evaluate();
			if (!object)
			{
				AML_ERROR("SizeOf object is null");
				return ParseResult::Failure;
			}

			uint64_t size = 0;
			switch (object->type)
			{
				case AML::Node::Type::Buffer:
					size = static_cast<AML::Buffer*>(object.ptr())->buffer.size();
					break;
				case AML::Node::Type::String:
					size = static_cast<AML::String*>(object.ptr())->string.size();
					break;
				case AML::Node::Type::Package:
					size = static_cast<AML::Package*>(object.ptr())->elements.size();
					break;
				default:
					AML_ERROR("SizeOf object is not a buffer, string or package ({})", static_cast<uint8_t>(object->type));
					return ParseResult::Failure;
			}

			return ParseResult(MUST(BAN::RefPtr<Integer>::create(size)));
		}
	};

}
