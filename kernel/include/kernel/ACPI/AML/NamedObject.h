#pragma once

#include <BAN/HashMap.h>
#include <kernel/ACPI/AML/Names.h>

namespace Kernel::ACPI::AML
{

	struct NamedObject : public AML::Node
	{
		BAN::RefPtr<NamedObject> parent;
		NameSeg name;

		NamedObject(Node::Type type, NameSeg name) : Node(type), name(name) {}
	};

	struct Name final : public AML::NamedObject
	{
		BAN::RefPtr<AML::Node> object;

		Name(NameSeg name, BAN::RefPtr<AML::Node> object)
			: NamedObject(Node::Type::Name, name), object(BAN::move(object))
		{}

		BAN::RefPtr<AML::Node> convert(uint8_t) override { return {}; }

		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> node) override
		{
			ASSERT(object);
			if (object->type == AML::Node::Type::Reference)
				return object->store(node);
			object = node;
			return node;
		}

		static ParseResult parse(ParseContext& context);
		virtual void debug_print(int indent) const override;
	};

}
