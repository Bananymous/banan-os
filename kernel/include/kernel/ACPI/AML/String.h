#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct String : public AML::Node
	{
		BAN::String string;

		String() : Node(Node::Type::String) {}

		BAN::RefPtr<AML::Node> evaluate() override
		{
			return this;
		}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::StringPrefix);
			context.aml_data = context.aml_data.slice(1);

			BAN::String string;

			while (context.aml_data.size() > 0)
			{
				if (context.aml_data[0] == 0x00)
					break;
				MUST(string.push_back(static_cast<char>(context.aml_data[0])));
				context.aml_data = context.aml_data.slice(1);
			}

			if (context.aml_data.size() == 0)
				return ParseResult::Failure;
			if (context.aml_data[0] != 0x00)
				return ParseResult::Failure;
			context.aml_data = context.aml_data.slice(1);

			auto string_node = MUST(BAN::RefPtr<String>::create());
			string_node->string = BAN::move(string);

			return ParseResult(string_node);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("String \"{}\"", string);
		}
	};

}
