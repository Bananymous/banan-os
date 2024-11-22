#pragma once

#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/String.h>

namespace Kernel::ACPI::AML
{

	struct Expression
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			switch (static_cast<Byte>(context.aml_data[0]))
			{
				// unary
				case AML::Byte::IncrementOp:
				case AML::Byte::DecrementOp:
				{
					auto opcode = (static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::IncrementOp) ? AML::Byte::AddOp : AML::Byte::SubtractOp;
					context.aml_data = context.aml_data.slice(1);

					auto source_result = AML::parse_object(context);
					if (!source_result.success())
						return ParseResult::Failure;
					auto conv_node = source_result.node() ? source_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
					if (!conv_node)
					{
						AML_ERROR("UnaryOp source not integer, type {}", static_cast<uint8_t>(source_result.node()->type));
						return ParseResult::Failure;
					}

					auto source_node = static_cast<AML::Integer*>(conv_node.ptr());
					if (source_node->constant)
					{
						AML_ERROR("UnaryOp source is constant");
						return ParseResult::Failure;
					}

					source_node->value += (opcode == AML::Byte::AddOp) ? 1 : -1;
					return ParseResult(source_node);
				}
				case AML::Byte::NotOp:
				{
					auto source_result = AML::parse_object(context);
					if (!source_result.success())
						return ParseResult::Failure;
					auto source_conv = source_result.node() ? source_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
					if (!source_conv)
					{
						AML_ERROR("NotOp source not an integer, type {}",
							static_cast<uint8_t>(source_result.node()->type)
						);
						if (source_result.node())
							source_result.node()->debug_print(1);
						AML_DEBUG_PRINTLN("");
						return ParseResult::Failure;
					}
					const auto source_value = static_cast<AML::Integer*>(source_conv.ptr())->value;

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
							AML_ERROR("NotOp target invalid");
							return ParseResult::Failure;
						}
					}

					auto result_node = MUST(BAN::RefPtr<AML::Integer>::create(~source_value));
					if (target_node && !target_node->store(result_node))
					{
						AML_ERROR("NotOp failed to store result");
						return ParseResult::Failure;
					}

					return ParseResult(result_node);
				}
				case AML::Byte::LNotOp:
				{
					context.aml_data = context.aml_data.slice(1);

					auto source_result = AML::parse_object(context);
					if (!source_result.success())
						return ParseResult::Failure;
					auto conv_node = source_result.node() ? source_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
					if (!conv_node)
					{
						AML_ERROR("UnaryOp source not integer, type {}", static_cast<uint8_t>(source_result.node()->type));
						return ParseResult::Failure;
					}
					auto source_node = static_cast<AML::Integer*>(conv_node.ptr());
					if (!source_node)
					{
						AML_ERROR("Logical NotOp source is not integer");
						return ParseResult::Failure;
					}

					auto result = source_node->value ? Integer::Constants::Zero : Integer::Constants::Ones;
					return ParseResult(result);
				}
				case AML::Byte::AddOp:
				case AML::Byte::AndOp:
				case AML::Byte::ModOp:
				case AML::Byte::MultiplyOp:
				case AML::Byte::NandOp:
				case AML::Byte::NorOp:
				case AML::Byte::OrOp:
				case AML::Byte::ShiftLeftOp:
				case AML::Byte::ShiftRightOp:
				case AML::Byte::SubtractOp:
				case AML::Byte::XorOp:
					return parse_binary_op(context);
				case AML::Byte::LAndOp:
				case AML::Byte::LEqualOp:
				case AML::Byte::LGreaterOp:
				case AML::Byte::LLessOp:
				case AML::Byte::LOrOp:
					return parse_logical_binary_op(context);
				case AML::Byte::DivideOp:
					AML_TODO("DivideOp");
					return ParseResult::Failure;
				default:
					ASSERT_NOT_REACHED();
			}
		}

	private:
		static ParseResult parse_binary_op(ParseContext& context)
		{
			auto opcode = static_cast<AML::Byte>(context.aml_data[0]);
			context.aml_data = context.aml_data.slice(1);

			auto lhs_result = AML::parse_object(context);
			if (!lhs_result.success())
				return ParseResult::Failure;
			auto lhs_conv = lhs_result.node() ? lhs_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
			if (!lhs_conv)
			{
				AML_ERROR("BinaryOP {2H} LHS not an integer, type {}",
					static_cast<uint8_t>(opcode),
					static_cast<uint8_t>(lhs_result.node()->type)
				);
				if (lhs_result.node())
					lhs_result.node()->debug_print(1);
				AML_DEBUG_PRINTLN("");
				return ParseResult::Failure;
			}
			const auto lhs_value = static_cast<AML::Integer*>(lhs_conv.ptr())->value;

			auto rhs_result = AML::parse_object(context);
			if (!rhs_result.success())
				return ParseResult::Failure;
			auto rhs_conv = rhs_result.node() ? rhs_result.node()->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
			if (!rhs_conv)
			{
				AML_ERROR("BinaryOP {2H} RHS not an integer", static_cast<uint8_t>(opcode));
				if (rhs_result.node())
					rhs_result.node()->debug_print(1);
				AML_DEBUG_PRINTLN("");
				return ParseResult::Failure;
			}
			const auto rhs_value = static_cast<AML::Integer*>(rhs_conv.ptr())->value;

			if (context.aml_data.size() < 1)
			{
				AML_ERROR("BinaryOP {2H} missing target", static_cast<uint8_t>(opcode));
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
					AML_ERROR("BinaryOP {2H} target invalid", static_cast<uint8_t>(opcode));
					return ParseResult::Failure;
				}
			}

			uint64_t (*func)(uint64_t, uint64_t) = nullptr;
			switch (opcode)
			{
				case AML::Byte::AddOp:			func = [](uint64_t a, uint64_t b) { return a + b; }; break;
				case AML::Byte::AndOp:			func = [](uint64_t a, uint64_t b) { return a & b; }; break;
				case AML::Byte::ModOp:			func = [](uint64_t a, uint64_t b) { return a % b; }; break;
				case AML::Byte::MultiplyOp:		func = [](uint64_t a, uint64_t b) { return a * b; }; break;
				case AML::Byte::NandOp:			func = [](uint64_t a, uint64_t b) { return ~(a & b); }; break;
				case AML::Byte::NorOp:			func = [](uint64_t a, uint64_t b) { return ~(a | b); }; break;
				case AML::Byte::OrOp:			func = [](uint64_t a, uint64_t b) { return a | b; }; break;
				case AML::Byte::ShiftLeftOp:	func = [](uint64_t a, uint64_t b) { return a << b; }; break;
				case AML::Byte::ShiftRightOp:	func = [](uint64_t a, uint64_t b) { return a >> b; }; break;
				case AML::Byte::SubtractOp:		func = [](uint64_t a, uint64_t b) { return a - b; }; break;
				case AML::Byte::XorOp:			func = [](uint64_t a, uint64_t b) { return a ^ b; }; break;
				default:
					ASSERT_NOT_REACHED();
			}

			auto result_node = MUST(BAN::RefPtr<AML::Integer>::create(func(lhs_value, rhs_value)));
			if (target_node && !target_node->store(result_node))
			{
				AML_ERROR("BinaryOp {2H} failed to store result", static_cast<uint8_t>(opcode));
				return ParseResult::Failure;
			}

			return ParseResult(result_node);
		}

		static ParseResult parse_logical_binary_op(ParseContext& context)
		{
			auto opcode = static_cast<AML::Byte>(context.aml_data[0]);
			context.aml_data = context.aml_data.slice(1);

			uint8_t mask;
			switch (opcode)
			{
				case AML::Byte::LAndOp:
				case AML::Byte::LOrOp:
					mask = AML::Node::ConvInteger;
					break;
				case AML::Byte::LEqualOp:
				case AML::Byte::LGreaterOp:
				case AML::Byte::LLessOp:
					mask = AML::Node::ConvInteger | AML::Node::ConvString | AML::Node::ConvBuffer;
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			auto lhs_result = AML::parse_object(context);
			if (!lhs_result.success())
				return ParseResult::Failure;
			auto lhs_node = lhs_result.node() ? lhs_result.node()->convert(mask) : BAN::RefPtr<AML::Node>();
			if (!lhs_node)
			{
				AML_TODO("Logical BinaryOP {2H} LHS evaluated to nothing", static_cast<uint8_t>(opcode));
				return ParseResult::Failure;
			}

			auto rhs_result = AML::parse_object(context);
			if (!rhs_result.success())
				return ParseResult::Failure;

			BAN::Optional<bool> result = false;
			switch (lhs_node->type)
			{
				case AML::Node::Type::Integer:
					result = static_cast<AML::Integer*>(lhs_node.ptr())->logical_compare(rhs_result.node(), opcode);
					break;
				case AML::Node::Type::Buffer:
					result = static_cast<AML::Buffer*>(lhs_node.ptr())->logical_compare(rhs_result.node(), opcode);
					break;
				case AML::Node::Type::String:
					result = static_cast<AML::String*>(lhs_node.ptr())->logical_compare(rhs_result.node(), opcode);
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			if (!result.has_value())
				return ParseResult::Failure;

			return ParseResult(result.value() ? AML::Integer::Constants::Ones : AML::Integer::Constants::Zero);
		}
	};

}
