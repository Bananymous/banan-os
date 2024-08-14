#pragma once

#include <BAN/Optional.h>
#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/Utils.h>

namespace Kernel::ACPI::AML
{

	struct Integer final : public AML::Node
	{
		struct Constants
		{
			// Initialized in Namespace::create_root_namespace
			static BAN::RefPtr<Integer> Zero;
			static BAN::RefPtr<Integer> One;
			static BAN::RefPtr<Integer> Ones;
		};

		uint64_t value;
		const bool constant;

		Integer(uint64_t value, bool constant = false)
			: Node(Node::Type::Integer)
			, value(value)
			, constant(constant)
		{}

		BAN::Optional<bool> logical_compare(BAN::RefPtr<AML::Node> node, AML::Byte binaryop)
		{
			auto rhs_node = node ? node->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
			if (!rhs_node)
			{
				AML_ERROR("Integer logical compare RHS cannot be converted to");
				return {};
			}
			const auto rhs_value = static_cast<AML::Integer*>(rhs_node.ptr())->value;

			switch (binaryop)
			{
				case AML::Byte::LAndOp:		return value && rhs_value;
				case AML::Byte::LEqualOp:	return value == rhs_value;
				case AML::Byte::LGreaterOp:	return value > rhs_value;
				case AML::Byte::LLessOp:	return value < rhs_value;
				case AML::Byte::LOrOp:		return value || rhs_value;
				default:
					ASSERT_NOT_REACHED();
			}
		}

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override
		{
			if (mask & AML::Node::ConvInteger)
				return this;
			if (mask & AML::Node::ConvBuffer)
			{
				AML_TODO("Convert Integer to Buffer");
				return {};
			}
			if (mask & AML::Node::ConvBufferField)
			{
				AML_TODO("Convert Integer to BufferField");
				return {};
			}
			if (mask & AML::Node::ConvFieldUnit)
			{
				AML_TODO("Convert Integer to FieldUnit");
				return {};
			}
			if (mask & AML::Node::ConvString)
			{
				AML_TODO("Convert Integer to String");
				return {};
			}
			return {};
		}

		BAN::RefPtr<Node> copy() override { return MUST(BAN::RefPtr<Integer>::create(value)); }

		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> store_node) override
		{
			if (constant)
			{
				AML_ERROR("Cannot store to constant integer");
				return {};
			}
			auto conv_node = store_node ? store_node->convert(AML::Node::ConvInteger) : BAN::RefPtr<AML::Node>();
			if (!conv_node)
			{
				AML_ERROR("Cannot store non-integer to integer");
				return {};
			}
			value = static_cast<AML::Integer*>(conv_node.ptr())->value;
			return MUST(BAN::RefPtr<AML::Integer>::create(value));
		}

		static ParseResult parse(BAN::ConstByteSpan& aml_data)
		{
			switch (static_cast<AML::Byte>(aml_data[0]))
			{
				case AML::Byte::ZeroOp:
					aml_data = aml_data.slice(1);
					return ParseResult(Constants::Zero);
				case AML::Byte::OneOp:
					aml_data = aml_data.slice(1);
					return ParseResult(Constants::One);
				case AML::Byte::OnesOp:
					aml_data = aml_data.slice(1);
					return ParseResult(Constants::Ones);
				case AML::Byte::BytePrefix:
				{
					if (aml_data.size() < 2)
						return ParseResult::Failure;
					const uint8_t value = aml_data[1];
					aml_data = aml_data.slice(2);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(value)));
				}
				case AML::Byte::WordPrefix:
				{
					if (aml_data.size() < 3)
						return ParseResult::Failure;
					uint16_t value = 0;
					value |= aml_data[1] << 0;
					value |= aml_data[2] << 8;
					aml_data = aml_data.slice(3);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(value)));
				}
				case AML::Byte::DWordPrefix:
				{
					if (aml_data.size() < 5)
						return ParseResult::Failure;
					uint32_t value = 0;
					value |= static_cast<uint32_t>(aml_data[1]) <<  0;
					value |= static_cast<uint32_t>(aml_data[2]) <<  8;
					value |= static_cast<uint32_t>(aml_data[3]) << 16;
					value |= static_cast<uint32_t>(aml_data[4]) << 24;
					aml_data = aml_data.slice(5);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(value)));
				}
				case AML::Byte::QWordPrefix:
				{
					if (aml_data.size() < 9)
						return ParseResult::Failure;
					uint64_t value = 0;
					value |= static_cast<uint64_t>(aml_data[1]) <<  0;
					value |= static_cast<uint64_t>(aml_data[2]) <<  8;
					value |= static_cast<uint64_t>(aml_data[3]) << 16;
					value |= static_cast<uint64_t>(aml_data[4]) << 24;
					value |= static_cast<uint64_t>(aml_data[5]) << 32;
					value |= static_cast<uint64_t>(aml_data[6]) << 40;
					value |= static_cast<uint64_t>(aml_data[7]) << 48;
					value |= static_cast<uint64_t>(aml_data[8]) << 56;
					aml_data = aml_data.slice(9);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(value)));
				}
				default:
					ASSERT_NOT_REACHED();
			}
		}

		void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			if (!constant)
				AML_DEBUG_PRINT("0x{H}", value);
			else
			{
				AML_DEBUG_PRINT("Const ");
				if (value == Constants::Zero->value)
					AML_DEBUG_PRINT("Zero");
				else if (value == Constants::One->value)
					AML_DEBUG_PRINT("One");
				else if (value == Constants::Ones->value)
					AML_DEBUG_PRINT("Ones");
				else
					ASSERT_NOT_REACHED();
			}
		}
	};

}
