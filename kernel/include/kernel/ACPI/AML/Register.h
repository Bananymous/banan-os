#pragma once

#include <kernel/ACPI/AML/Node.h>

namespace Kernel::ACPI::AML
{

	struct Register final : public AML::Node
	{
		BAN::RefPtr<AML::Node> value;

		Register()
			: Node(Node::Type::Register)
		{}
		Register(BAN::RefPtr<AML::Node> value)
			: Node(Node::Type::Register)
			, value(value)
		{}

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override
		{
			if (!value)
			{
				AML_ERROR("Trying to convert null Register");
				return {};
			}
			return value->convert(mask);
		}

		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> source) override
		{
			if (value && value->type == AML::Node::Type::Reference)
				return value->store(source);
			value = source->copy();
			return value;
		}

		void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			if (!value)
				AML_DEBUG_PRINT("Register { No value }");
			else
			{
				AML_DEBUG_PRINTLN("Register { ");
				value->debug_print(indent + 1);
				AML_DEBUG_PRINTLN("");
				AML_DEBUG_PRINT_INDENT(indent);
				AML_DEBUG_PRINT(" }");
			}
		}
	};

}
