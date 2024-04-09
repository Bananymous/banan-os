#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	struct Method : public AML::Scope
	{
		uint8_t arg_count;
		bool serialized;
		uint8_t sync_level;

		BAN::ConstByteSpan term_list;

		Method(AML::NameSeg name, uint8_t arg_count, bool serialized, uint8_t sync_level)
			: AML::Scope(Node::Type::Method, name)
			, arg_count(arg_count)
			, serialized(serialized)
			, sync_level(sync_level)
		{}

		static ParseResult parse(AML::ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::MethodOp);
			context.aml_data = context.aml_data.slice(1);

			auto method_pkg = AML::parse_pkg(context.aml_data);
			if (!method_pkg.has_value())
				return ParseResult::Failure;

			auto name_string = AML::NameString::parse(method_pkg.value());
			if (!name_string.has_value())
				return ParseResult::Failure;

			if (method_pkg->size() < 1)
				return ParseResult::Failure;
			auto method_flags = method_pkg.value()[0];
			method_pkg = method_pkg.value().slice(1);

			auto method = MUST(BAN::RefPtr<Method>::create(
				name_string.value().path.back(),
				method_flags & 0x07,
				(method_flags >> 3) & 0x01,
				method_flags >> 4
			));
			if (!context.root_namespace->add_named_object(context, name_string.value(), method))
				return ParseResult::Failure;

			auto method_scope = context.root_namespace->resolve_path(context.scope, name_string.value());
			if (!method_scope.has_value())
				return ParseResult::Failure;
			method->term_list = method_pkg.value();
			method->scope = method_scope.release_value();

#if AML_DEBUG_LEVEL >= 2
			method->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult::Success;
		}

		BAN::Optional<BAN::RefPtr<AML::Node>> evaluate(BAN::RefPtr<AML::Namespace> root_namespace)
		{
			ParseContext context;
			context.root_namespace = root_namespace.ptr();
			context.aml_data = term_list;
			context.scope = scope;

			AML_DEBUG_PRINTLN("Evaluating method {}", scope);

			BAN::Optional<BAN::RefPtr<AML::Node>> return_value;

			ASSERT(arg_count == 0);
			while (context.aml_data.size() > 0)
			{
				if (static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ReturnOp)
				{
					context.aml_data = context.aml_data.slice(1);
					auto result = AML::parse_object(context);
					if (result.success())
						return_value = result.node();
					break;
				}

				auto object_result = AML::parse_object(context);
				if (!object_result.success())
					break;
			}

			while (!context.created_objects.empty())
			{
				root_namespace->remove_named_object(context.created_objects.back());
				context.created_objects.pop_back();
			}

			return return_value;
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Method ");
			name.debug_print();
			AML_DEBUG_PRINTLN("({} args, {}Serialized, 0x{H}) {", arg_count, serialized ? "" : "Not", sync_level);
			AML_DEBUG_PRINT_INDENT(indent + 1);
			AML_DEBUG_PRINTLN("TermList: {} bytes", term_list.size());
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
