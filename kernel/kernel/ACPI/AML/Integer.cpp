#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/String.h>

namespace Kernel::ACPI
{

	AML::Integer::Integer(uint64_t value, bool constant)
		: Node(Node::Type::Integer)
		, value(value)
		, constant(constant)
	{}

	BAN::Optional<bool> AML::Integer::logical_compare(BAN::RefPtr<AML::Node> node, AML::Byte binaryop)
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

	BAN::RefPtr<AML::Node> AML::Integer::convert(uint8_t mask)
	{
		if (mask & AML::Node::ConvInteger)
			return this;
		if (mask & AML::Node::ConvBuffer)
		{
			auto buffer = MUST(BAN::RefPtr<AML::Buffer>::create());
			MUST(buffer->buffer.resize(8));
			for (size_t i = 0; i < 8; i++)
				buffer->buffer[i] = (value >> (56 - i * 8)) & 0xFF;
			return buffer;
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
			constexpr auto get_hex_char =
				[](uint8_t nibble)
				{
					return (nibble < 10 ? '0' : 'A' - 10) + nibble;
				};

			auto string = MUST(BAN::RefPtr<AML::String>::create());
			MUST(string->string.resize(16));
			for (size_t i = 0; i < 16; i++)
				string->string[i] = get_hex_char((value >> (60 - i * 4)) & 0xF);
			return string;
		}
		return {};
	}

	BAN::RefPtr<AML::Node> AML::Integer::copy()
	{
		return MUST(BAN::RefPtr<Integer>::create(value));
	}

	BAN::RefPtr<AML::Node> AML::Integer::store(BAN::RefPtr<AML::Node> store_node)
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

	AML::ParseResult AML::Integer::parse(BAN::ConstByteSpan& aml_data)
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

	void AML::Integer::debug_print(int indent) const
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

}
