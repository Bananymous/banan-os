#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct String final : public AML::Node
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

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override;

		BAN::RefPtr<AML::Node> copy() override
		{
			auto new_string = MUST(BAN::RefPtr<AML::String>::create());
			MUST(new_string->string.resize(this->string.size()));
			for (size_t i = 0; i < this->string.size(); i++)
				new_string->string[i] = this->string[i];
			return new_string;
		}

		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> node) override
		{
			ASSERT(node);
			auto conv_node = node->convert(AML::Node::ConvString);
			if (!conv_node)
			{
				AML_ERROR("Could not convert to String");
				return {};
			}
			auto* string_node = static_cast<AML::String*>(conv_node.ptr());
			MUST(string.resize(string_node->string.size()));
			for (size_t i = 0; i < string.size(); i++)
				string[i] = string_node->string[i];
			return string_node->copy();
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

	private:
		BAN::RefPtr<AML::Buffer> as_buffer();
	};

}
