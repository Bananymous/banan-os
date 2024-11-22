#pragma once

#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/String.h>

namespace Kernel::ACPI::AML
{

	struct Concat
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::ConcatOp);
			context.aml_data = context.aml_data.slice(1);

			auto source1_result = AML::parse_object(context);
			if (!source1_result.success())
				return ParseResult::Failure;
			auto source1 = source1_result.node() ? source1_result.node()->to_underlying() : BAN::RefPtr<AML::Node>();

			auto source2_result = AML::parse_object(context);
			if (!source2_result.success())
				return ParseResult::Failure;
			auto source2 = source1_result.node() ? source1_result.node()->to_underlying() : BAN::RefPtr<AML::Node>();

			if (!source1 || !source2)
			{
				AML_ERROR("ConcatOp sources could not be parsed");
				return ParseResult::Failure;
			}

			switch (source1->type)
			{
				case AML::Node::Type::Integer:
					source1 = source1->convert(AML::Node::ConvBuffer);
					source2 = source2->convert(AML::Node::ConvBuffer);
					break;
				case AML::Node::Type::String:
					source2 = source2->convert(AML::Node::ConvString);
					break;
				case AML::Node::Type::Buffer:
					source2 = source2->convert(AML::Node::ConvBuffer);
					break;
				default:
					source1 = source1->convert(AML::Node::ConvString);
					source2 = source2->convert(AML::Node::ConvString);
					break;
			}

			if (!source1 || !source2)
			{
				AML_ERROR("ConcatOp sources could not be converted");
				return ParseResult::Failure;
			}

			ASSERT(source1->type == source2->type);

			BAN::RefPtr<AML::Node> result;
			BAN::Vector<uint8_t>* result_data  = nullptr;
			BAN::Vector<uint8_t>* source1_data = nullptr;
			BAN::Vector<uint8_t>* source2_data = nullptr;

			switch (source1->type)
			{
				case AML::Node::Type::String:
					result = MUST(BAN::RefPtr<AML::String>::create());
					result_data = &static_cast<AML::String*>(result.ptr())->string;
					source1_data = &static_cast<AML::String*>(source1.ptr())->string;
					source2_data = &static_cast<AML::String*>(source2.ptr())->string;
					break;
				case AML::Node::Type::Buffer:
					result = MUST(BAN::RefPtr<AML::Buffer>::create());
					result_data = &static_cast<AML::Buffer*>(result.ptr())->buffer;
					source1_data = &static_cast<AML::Buffer*>(source1.ptr())->buffer;
					source2_data = &static_cast<AML::Buffer*>(source2.ptr())->buffer;
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			ASSERT(result_data && source1_data && source2_data);

			MUST(result_data->resize(source1_data->size() + source2_data->size()));
			for (size_t i = 0; i < source1_data->size(); i++)
				(*result_data)[i] = (*source1_data)[i];
			for (size_t i = 0; i < source2_data->size(); i++)
				(*result_data)[source1_data->size() + i] = (*source2_data)[i];

#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINT("Concat ");
			source1->debug_print(0);
			AML_DEBUG_PRINT(", ");
			source2->debug_print(0);
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
