#pragma once

#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/Names.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct Alias final : public AML::NamedObject
	{
		BAN::RefPtr<AML::Node> target;

		Alias(NameSeg name, BAN::RefPtr<AML::Node> target)
			: NamedObject(Node::Type::Alias, name)
			, target(target)
		{}

		BAN::RefPtr<AML::Node> named_target() override { return target; }

		bool is_scope() const override { return target->is_scope(); }

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override
		{
			ASSERT(target);
			return target->convert(mask);
		}

		BAN::RefPtr<Node> copy() override
		{
			ASSERT(target);
			return target->copy();
		}

		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> node) override
		{
			ASSERT(target);
			return target->store(node);
		}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::AliasOp);
			context.aml_data = context.aml_data.slice(1);

			auto source_string = AML::NameString::parse(context.aml_data);
			if (!source_string.has_value())
				return ParseResult::Failure;

			auto source_object = AML::Namespace::root_namespace()->find_object(context.scope, source_string.value(), AML::Namespace::FindMode::Normal);
			auto alias_string = AML::NameString::parse(context.aml_data);
			if (!alias_string.has_value())
				return ParseResult::Failure;

			if (!source_object)
			{
				AML_PRINT("Alias target could not be found");
				return ParseResult::Success;
			}

			auto alias = MUST(BAN::RefPtr<Alias>::create(alias_string.value().path.back(), source_object->named_target()));
			if (!Namespace::root_namespace()->add_named_object(context, alias_string.value(), alias))
				return ParseResult::Success;

	#if AML_DEBUG_LEVEL >= 2
			alias->debug_print(0);
			AML_DEBUG_PRINTLN("");
	#endif

			return ParseResult::Success;
		}

		void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINTLN("Alias {} { ", name);
			target->debug_print(indent + 1);
			AML_DEBUG_PRINTLN("");
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
