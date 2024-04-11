#pragma once

#include <BAN/Endianness.h>
#include <BAN/Optional.h>
#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/Utils.h>

namespace Kernel::ACPI::AML
{

	struct Integer : public Node
	{
		static constexpr uint64_t Ones = -1;
		const uint64_t value;

		Integer(uint64_t value)
			: Node(Node::Type::Integer)
			, value(value)
		{}

		BAN::RefPtr<AML::Node> evaluate() override
		{
			return this;
		}

		static ParseResult parse(BAN::ConstByteSpan& aml_data)
		{
			switch (static_cast<AML::Byte>(aml_data[0]))
			{
				case AML::Byte::ZeroOp:
					aml_data = aml_data.slice(1);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(0)));
				case AML::Byte::OneOp:
					aml_data = aml_data.slice(1);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(1)));
				case AML::Byte::OnesOp:
					aml_data = aml_data.slice(1);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(Ones)));
				case AML::Byte::BytePrefix:
				{
					if (aml_data.size() < 2)
						return ParseResult::Failure;
					uint8_t value = aml_data[1];
					aml_data = aml_data.slice(2);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(value)));
				}
				case AML::Byte::WordPrefix:
				{
					if (aml_data.size() < 3)
						return ParseResult::Failure;
					uint16_t value = BAN::little_endian_to_host<uint16_t>(
						*reinterpret_cast<const uint16_t*>(&aml_data[1])
					);
					aml_data = aml_data.slice(3);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(value)));
				}
				case AML::Byte::DWordPrefix:
				{
					if (aml_data.size() < 5)
						return ParseResult::Failure;
					uint32_t value = BAN::little_endian_to_host<uint32_t>(
						*reinterpret_cast<const uint32_t*>(&aml_data[1])
					);
					aml_data = aml_data.slice(5);
					return ParseResult(MUST(BAN::RefPtr<Integer>::create(value)));
				}
				case AML::Byte::QWordPrefix:
				{
					if (aml_data.size() < 9)
						return ParseResult::Failure;
					uint64_t value = BAN::little_endian_to_host<uint64_t>(
						*reinterpret_cast<const uint64_t*>(&aml_data[1])
					);
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
			if (value == Ones)
				AML_DEBUG_PRINT("Ones");
			else
				AML_DEBUG_PRINT("0x{H}", value);
		}
	};

}
