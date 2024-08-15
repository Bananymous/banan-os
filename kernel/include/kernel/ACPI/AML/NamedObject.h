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

		[[nodiscard]] virtual BAN::RefPtr<AML::Node> named_target() { return this; }
	};

	struct Name final : public AML::NamedObject
	{
		BAN::RefPtr<AML::Node> object;

		Name(NameSeg name, BAN::RefPtr<AML::Node> object)
			: NamedObject(Node::Type::Name, name), object(BAN::move(object))
		{}

		BAN::RefPtr<AML::Node> named_target() override { return object; }

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override
		{
			ASSERT(object);
			return object->convert(mask);
		}

		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> node) override
		{
			ASSERT(object);
			ASSERT(object->type != AML::Node::Type::Reference);
			return object->store(node);
		}

		static ParseResult parse(ParseContext& context);
		virtual void debug_print(int indent) const override;
	};

}
