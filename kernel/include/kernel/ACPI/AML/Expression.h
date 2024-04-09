#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>

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
				case AML::Byte::NotOp:
				case AML::Byte::LNotOp:
					AML_TODO("Expression {2H}", context.aml_data[0]);
					return ParseResult::Failure;
				// binary
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
				case AML::Byte::LAndOp:
				case AML::Byte::LEqualOp:
				case AML::Byte::LGreaterOp:
				case AML::Byte::LLessOp:
				case AML::Byte::LOrOp:
				{
					auto opcode = static_cast<AML::Byte>(context.aml_data[0]);
					context.aml_data = context.aml_data.slice(1);

					auto lhs_result = AML::parse_object(context);
					if (!lhs_result.success())
						return ParseResult::Failure;
					auto lhs_node = lhs_result.node();
					if (!lhs_node)
					{
						AML_ERROR("LHS object is null");
						return ParseResult::Failure;
					}
					auto lhs = lhs_node->evaluate();
					if (!lhs)
					{
						AML_ERROR("Failed to evaluate LHS object");
						return ParseResult::Failure;
					}

					auto rhs_result = AML::parse_object(context);
					if (!rhs_result.success())
						return ParseResult::Failure;
					auto rhs_node = rhs_result.node();
					if (!rhs_node)
					{
						AML_ERROR("RHS object is null");
						return ParseResult::Failure;
					}
					auto rhs = rhs_node->evaluate();
					if (!rhs)
					{
						AML_ERROR("Failed to evaluate RHS object");
						return ParseResult::Failure;
					}

					return parse_binary_op(context, opcode, lhs, rhs);
				}
				// trinary
				case AML::Byte::DivideOp:
					AML_TODO("Expression {2H}", context.aml_data[0]);
					return ParseResult::Failure;
				default:
					ASSERT_NOT_REACHED();
			}
		}

	private:
		static ParseResult parse_binary_op(ParseContext& context, AML::Byte opcode, BAN::RefPtr<AML::Node> lhs_node, BAN::RefPtr<AML::Node> rhs_node)
		{
			if (lhs_node->type != AML::Node::Type::Integer)
			{
				AML_TODO("LHS object is not an integer, type {}", static_cast<uint8_t>(lhs_node->type));
				return ParseResult::Failure;
			}
			if (rhs_node->type != AML::Node::Type::Integer)
			{
				AML_TODO("RHS object is not an integer, type {}", static_cast<uint8_t>(rhs_node->type));
				return ParseResult::Failure;
			}

			bool logical = false;
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
				case AML::Byte::LAndOp:			func = [](uint64_t a, uint64_t b) { return a && b ? Integer::Ones : 0; }; logical = true; break;
				case AML::Byte::LEqualOp:		func = [](uint64_t a, uint64_t b) { return a == b ? Integer::Ones : 0; }; logical = true; break;
				case AML::Byte::LGreaterOp:		func = [](uint64_t a, uint64_t b) { return a > b  ? Integer::Ones : 0; }; logical = true; break;
				case AML::Byte::LLessOp:		func = [](uint64_t a, uint64_t b) { return a < b  ? Integer::Ones : 0; }; logical = true; break;
				case AML::Byte::LOrOp:			func = [](uint64_t a, uint64_t b) { return a || b ? Integer::Ones : 0; }; logical = true; break;
				default:
					ASSERT_NOT_REACHED();
			}

			uint64_t lhs = static_cast<AML::Integer*>(lhs_node.ptr())->value;
			uint64_t rhs = static_cast<AML::Integer*>(rhs_node.ptr())->value;
			uint64_t result = func(lhs, rhs);

			auto result_node = MUST(BAN::RefPtr<AML::Integer>::create(result));

			if (!logical)
			{
				if (context.aml_data.size() < 1)
					return ParseResult::Failure;
				if (context.aml_data[0] == 0x00)
					context.aml_data = context.aml_data.slice(1);
				else
				{
					auto target_result = AML::parse_object(context);
					if (!target_result.success())
						return ParseResult::Failure;
					auto target = target_result.node();
					if (!target)
					{
						AML_ERROR("Target object is null");
						return ParseResult::Failure;
					}
					if (!target->store(result_node))
					{
						AML_ERROR("Failed to store result");
						return ParseResult::Failure;
					}
				}
			}

			return ParseResult(result_node);
		}
	};

}
