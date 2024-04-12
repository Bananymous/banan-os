#pragma once

#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Reference.h>

namespace Kernel::ACPI::AML
{

	struct Index
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::IndexOp);
			context.aml_data = context.aml_data.slice(1);

			auto source_result = AML::parse_object(context);
			if (!source_result.success())
				return ParseResult::Failure;
			auto source = source_result.node() ? source_result.node()->evaluate() : BAN::RefPtr<AML::Node>();
			if (!source)
			{
				AML_ERROR("IndexOp source is null");
				return ParseResult::Failure;
			}

			auto index_result = AML::parse_object(context);
			if (!index_result.success())
				return ParseResult::Failure;
			auto index = index_result.node() ? index_result.node()->as_integer() : BAN::Optional<uint64_t>();
			if (!index.has_value())
			{
				AML_ERROR("IndexOp index is not an integer");
				return ParseResult::Failure;
			}

			BAN::RefPtr<AML::Reference> result;
			switch (source->type)
			{
				case AML::Node::Type::Buffer:
				{
					auto buffer = static_cast<AML::Buffer*>(source.ptr());
					if (index.value() >= buffer->buffer.size())
					{
						AML_ERROR("IndexOp index is out of buffer bounds");
						return ParseResult::Failure;
					}
					auto buffer_field = MUST(BAN::RefPtr<BufferField>::create(NameSeg(""sv), buffer, index.value() * 8, 8));
					result = MUST(BAN::RefPtr<AML::Reference>::create(buffer_field));
					break;
				}
				case AML::Node::Type::Package:
					AML_TODO("IndexOp source Package");
					return ParseResult::Failure;
				case AML::Node::Type::String:
					AML_TODO("IndexOp source String");
					return ParseResult::Failure;
				default:
					AML_ERROR("IndexOp source is not a Buffer, Package, or String");
					return ParseResult::Failure;
			}

#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINT("Index {}, ", index.value());
			source->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			if (context.aml_data.size() < 1)
				return ParseResult::Failure;

			if (context.aml_data[0] == 0x00)
				context.aml_data = context.aml_data.slice(1);
			else
			{
				auto destination_result = AML::parse_object(context);
				if (!destination_result.success())
					return ParseResult::Failure;
				auto destination = destination_result.node();
				if (!destination)
				{
					AML_ERROR("IndexOp failed to resolve destination");
					return ParseResult::Failure;
				}

				if (!destination->store(result))
					return ParseResult::Failure;
			}

			return ParseResult(result);
		}
	};

}