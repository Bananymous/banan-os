#include <kernel/ACPI/AML/Reference.h>
#include <kernel/ACPI/AML/Register.h>

namespace Kernel::ACPI
{

	AML::Register::Register()
		: Node(Node::Type::Register)
	{}
	AML::Register::Register(BAN::RefPtr<AML::Node> node)
		: Node(Node::Type::Register)
	{
		if (!node)
		{
			value = node;
			return;
		}

		while (node)
		{
			if (node->type == AML::Node::Type::Reference)
				node = static_cast<AML::Reference*>(node.ptr())->node;
			else if (node->type != AML::Node::Type::Buffer && node->type != AML::Node::Type::Package)
				node = node->copy();
			if (node->type == AML::Node::Type::Register)
			{
				node = static_cast<AML::Register*>(node.ptr())->value;
				continue;
			}
			break;
		}
		ASSERT(node);
		value = node;
	}

	BAN::RefPtr<AML::Node> AML::Register::convert(uint8_t mask)
	{
		if (!value)
		{
			AML_ERROR("Trying to convert null Register");
			return {};
		}
		return value->convert(mask);
	}

	BAN::RefPtr<AML::Node> AML::Register::store(BAN::RefPtr<AML::Node> source)
	{
		if (source && source->type == AML::Node::Type::Register)
			source = static_cast<AML::Register*>(source.ptr())->value;
		if (value && value->type == AML::Node::Type::Reference)
			return value->store(source);
		value = source->copy();
		return value;
	}

	void AML::Register::debug_print(int indent) const
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

}
