#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/Names.h>
#include <kernel/ACPI/AML/String.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct Reference : public AML::Node
	{
		BAN::RefPtr<AML::Node> node;

		Reference(BAN::RefPtr<AML::Node> node)
			: Node(AML::Node::Type::Reference)
			, node(node)
		{
			ASSERT(node);
		}

		BAN::RefPtr<AML::Node> evaluate() override
		{
			return this;
		}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);

			bool conditional = false;
			switch (static_cast<AML::Byte>(context.aml_data[0]))
			{
				case AML::Byte::DerefOfOp:
					return parse_dereference(context);
				case AML::Byte::RefOfOp:
					context.aml_data = context.aml_data.slice(1);
					conditional = false;
					break;
				case AML::Byte::ExtOpPrefix:
					ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::CondRefOfOp);
					context.aml_data = context.aml_data.slice(2);
					conditional = true;
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			BAN::RefPtr<AML::Node> object;
			if (NameString::can_parse(context.aml_data))
			{
				auto name = NameString::parse(context.aml_data);
				if (!name.has_value())
					return ParseResult::Failure;
				object = Namespace::root_namespace()->find_object(context.scope, name.value(), Namespace::FindMode::Normal);
			}
			else
			{
				auto parse_result = AML::parse_object(context);
				if (!parse_result.success())
					return ParseResult::Failure;
				object = parse_result.node();
			}

			if (!conditional)
			{
				if (!object)
				{
					AML_ERROR("RefOf failed to resolve reference");
					return ParseResult::Failure;
				}
				auto reference = MUST(BAN::RefPtr<Reference>::create(object));
#if AML_DEBUG_LEVEL >= 2
				reference->debug_print(0);
				AML_DEBUG_PRINTLN("");
#endif
				return ParseResult(reference);
			}

			if (context.aml_data.size() >= 1 && context.aml_data[0] != 0x00)
			{
				auto target_result = AML::parse_object(context);
				if (!target_result.success())
					return ParseResult::Failure;
				auto target_node = target_result.node();
				if (!target_node)
				{
					AML_ERROR("CondRefOf failed to resolve target");
					return ParseResult::Failure;
				}
				target_node->store(MUST(BAN::RefPtr<Reference>::create(object)));
			}

#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINT("CondRefOf ");
			if (object)
				object->debug_print(0);
			else
				AML_DEBUG_PRINT("null");
			AML_DEBUG_PRINTLN("");
#endif

			auto return_value = object ? Integer::Constants::Ones : Integer::Constants::Zero;
			return AML::ParseResult(return_value);
		}

		static ParseResult parse_dereference(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::DerefOfOp);
			context.aml_data = context.aml_data.slice(1);

			if (context.aml_data.size() >= 1 && static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::StringPrefix)
			{
				auto string_result = AML::String::parse(context);
				if (!string_result.success())
					return ParseResult::Failure;
				ASSERT(string_result.node());
				auto string = static_cast<AML::String*>(string_result.node().ptr());
				AML_TODO("DerefOf String ({})", string->string);
				return ParseResult::Failure;
			}
			else
			{
				auto parse_result = AML::parse_object(context);
				if (!parse_result.success())
					return ParseResult::Failure;
				auto object = parse_result.node();
				if (!object || object->type != AML::Node::Type::Reference)
				{
					AML_TODO("DerefOf source is not a Reference, but a {}", object ? static_cast<uint8_t>(object->type) : 999);
					return ParseResult::Failure;
				}
#if AML_DEBUG_LEVEL >= 2
				AML_DEBUG_PRINT("DerefOf ");
				object->debug_print(0);
				AML_DEBUG_PRINTLN("");
#endif
				return ParseResult(static_cast<Reference*>(object.ptr())->node);
			}
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINTLN("Reference {");
			node->debug_print(indent + 1);
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
