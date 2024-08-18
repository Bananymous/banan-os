#pragma once

#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/Package.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Reference.h>
#include <kernel/ACPI/AML/Register.h>

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
			auto source = source_result.node() ? source_result.node()->to_underlying() : BAN::RefPtr<AML::Node>();
			if (source && source->type != AML::Node::Type::Package)
				source = source->convert(AML::Node::ConvBuffer | AML::Node::ConvInteger | AML::Node::ConvString);
			if (!source)
			{
				AML_ERROR("IndexOp source could not be converted");
				return ParseResult::Failure;
			}

			auto index_result = AML::parse_object(context);
			if (!index_result.success())
				return ParseResult::Failure;
			auto index_node = index_result.node()
				? index_result.node()->convert(AML::Node::ConvInteger)
				: BAN::RefPtr<AML::Node>();
			if (!index_node)
			{
				AML_ERROR("IndexOp index is not an integer");
				return ParseResult::Failure;
			}
			const auto index = static_cast<AML::Integer*>(index_node.ptr())->value;

			BAN::RefPtr<AML::Reference> result;
			switch (source->type)
			{
				case AML::Node::Type::Buffer:
				{
					auto buffer = BAN::RefPtr<AML::Buffer>(static_cast<AML::Buffer*>(source.ptr()));
					if (index >= buffer->buffer.size())
					{
						AML_ERROR("IndexOp index is out of buffer bounds");
						return ParseResult::Failure;
					}
					auto buffer_field = MUST(BAN::RefPtr<BufferField>::create(NameSeg(""_sv), buffer, index * 8, 8));
					result = MUST(BAN::RefPtr<AML::Reference>::create(buffer_field));
					break;
				}
				case AML::Node::Type::Package:
				{
					auto package = static_cast<AML::Package*>(source.ptr());
					if (index >= package->elements.size())
					{
						AML_ERROR("IndexOp index is out of package bounds");
						return ParseResult::Failure;
					}
					auto package_element = package->elements[index];
					result = MUST(BAN::RefPtr<AML::Reference>::create(package_element->to_underlying()));
					break;
				}
				case AML::Node::Type::String:
				{
					auto string = BAN::RefPtr<AML::String>(static_cast<AML::String*>(source.ptr()));
					if (index >= string->string.size())
					{
						AML_ERROR("IndexOp index is out of string bounds");
						return ParseResult::Failure;
					}
					auto buffer_field = MUST(BAN::RefPtr<BufferField>::create(NameSeg(""_sv), string, index * 8, 8));
					result = MUST(BAN::RefPtr<AML::Reference>::create(buffer_field));
					break;
				}
				default:
					AML_ERROR("IndexOp source is not a Buffer, Package, or String");
					return ParseResult::Failure;
			}

#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINT("Index {}, ", index);
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
