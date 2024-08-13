#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct String : public AML::Node
	{
		BAN::Vector<uint8_t> string;

		String() : Node(Node::Type::String) {}
		String(BAN::StringView string)
			: Node(Node::Type::String)
		{
			MUST(this->string.resize(string.size()));
			for (size_t i = 0; i < string.size(); i++)
				this->string[i] = string[i];
		}

		BAN::Optional<bool> logical_compare(BAN::RefPtr<AML::Node> node, AML::Byte binaryop);

		BAN::RefPtr<AML::Buffer> as_buffer() override;

		BAN::RefPtr<AML::Node> evaluate() override
		{
			return this;
		}

		BAN::StringView string_view() const
		{
			return BAN::StringView(reinterpret_cast<const char*>(string.data()), string.size());
		}

		static ParseResult parse(ParseContext& context);

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("String \"{}\"", string_view());
		}
	};

}
