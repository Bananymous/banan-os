#pragma once

#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/String.h>

namespace Kernel::ACPI::AML
{

	struct Conversion
	{
		static ParseResult parse(AML::ParseContext& context)
		{
			const auto opcode = static_cast<AML::Byte>(context.aml_data[0]);
			context.aml_data = context.aml_data.slice(1);

			switch (opcode)
			{
				case AML::Byte::ToIntegerOp:
				case AML::Byte::ToBufferOp:
				case AML::Byte::ToStringOp:
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			auto data_result = AML::parse_object(context);
			if (!data_result.success())
				return ParseResult::Failure;
			auto data_node = data_result.node();
			if (!data_node)
			{
				AML_ERROR("Conversion {2H} data could not be evaluated", static_cast<uint8_t>(opcode));
				return ParseResult::Failure;
			}

			if (context.aml_data.size() < 1)
			{
				AML_ERROR("Conversion {2H} missing target", static_cast<uint8_t>(opcode));
				return ParseResult::Failure;
			}

			BAN::RefPtr<AML::Node> target_node;
			if (context.aml_data[0] == 0x00)
				context.aml_data = context.aml_data.slice(1);
			else
			{
				auto target_result = AML::parse_object(context);
				if (!target_result.success())
					return ParseResult::Failure;
				target_node = target_result.node();
				if (!target_node)
				{
					AML_ERROR("Conversion {2H} target invalid", static_cast<uint8_t>(opcode));
					return ParseResult::Failure;
				}
			}

			BAN::RefPtr<AML::Node> converted;
			switch (opcode)
			{
				case AML::Byte::ToBufferOp:
					converted = data_node->as_buffer();
					break;
				case AML::Byte::ToIntegerOp:
					converted = data_node->as_integer();
					break;
				case AML::Byte::ToStringOp:
					converted = data_node->as_string();
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			if (!converted)
			{
				AML_ERROR("Conversion {2H} could not convert from node type {}", static_cast<uint8_t>(opcode), static_cast<uint8_t>(data_node->type));
				return ParseResult::Failure;
			}

			if (target_node && !target_node->store(converted))
			{
				AML_ERROR("Conversion {2H} failed to store converted value", static_cast<uint8_t>(opcode));
				return ParseResult::Failure;
			}

			return ParseResult(converted);
		}
	};

}
