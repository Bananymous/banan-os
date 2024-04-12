#pragma once

#include <BAN/HashMap.h>
#include <kernel/ACPI/AML/Names.h>

namespace Kernel::ACPI::AML
{

	struct NamedObject : public Node
	{
		BAN::RefPtr<NamedObject> parent;
		NameSeg name;

		NamedObject(Node::Type type, NameSeg name) : Node(type), name(name) {}
	};

	struct Name : public NamedObject
	{
		BAN::RefPtr<AML::Node> object;

		Name(NameSeg name, BAN::RefPtr<AML::Node> object)
			: NamedObject(Node::Type::Name, name), object(BAN::move(object))
		{}

		BAN::RefPtr<AML::Node> evaluate() override
		{
			ASSERT(object);
			return object->evaluate();
		}

		bool store(BAN::RefPtr<AML::Node> node) override
		{
			ASSERT(object);
			object = node;
			return true;
		}

		static ParseResult parse(ParseContext& context);
		virtual void debug_print(int indent) const override;
	};

}
