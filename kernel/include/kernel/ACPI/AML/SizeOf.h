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
			auto object_node = object_result.node();
			if (object_node && object_node->type == AML::Node::Type::Register)
				object_node = static_cast<AML::Register*>(object_node.ptr())->value;
			if (!object_node)
			{
				AML_ERROR("SizeOf object is null");
				return ParseResult::Failure;
			}
			if (object_node->type != AML::Node::Type::Package)
				object_node = object_node->convert(AML::Node::ConvBuffer | AML::Node::ConvString);
			if (!object_node)
			{
				AML_ERROR("SizeOf object is not Buffer, String or Package");
				return ParseResult::Failure;
			}

			uint64_t size = 0;
			switch (object_node->type)
			{
				case AML::Node::Type::Buffer:
					size = static_cast<AML::Buffer*>(object_node.ptr())->buffer.size();
					break;
				case AML::Node::Type::String:
					size = static_cast<AML::String*>(object_node.ptr())->string.size();
					break;
				case AML::Node::Type::Package:
					size = static_cast<AML::Package*>(object_node.ptr())->elements.size();
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			return ParseResult(MUST(BAN::RefPtr<Integer>::create(size)));
		}
	};

}
