#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI
{

	AML::ParseResult AML::Scope::parse(ParseContext& context)
	{
		ASSERT(context.aml_data.size() >= 1);
		ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ScopeOp);
		context.aml_data = context.aml_data.slice(1);

		auto scope_pkg = AML::parse_pkg(context.aml_data);
		if (!scope_pkg.has_value())
			return ParseResult::Failure;

		auto name_string = AML::NameString::parse(scope_pkg.value());
		if (!name_string.has_value())
			return ParseResult::Failure;

		auto named_object = Namespace::root_namespace()->find_object(context.scope, name_string.value());
		if (!named_object)
		{
			AML_ERROR("Scope name {} not found in namespace", name_string.value());
			return ParseResult::Failure;
		}
		if (!named_object->is_scope())
		{
			AML_ERROR("Scope name {} does not name a namespace", name_string.value());
			return ParseResult::Failure;
		}

		auto* scope = static_cast<Scope*>(named_object.ptr());
		return scope->enter_context_and_parse_term_list(context, name_string.value(), scope_pkg.value());
	}

	AML::ParseResult AML::Scope::enter_context_and_parse_term_list(ParseContext& outer_context, const AML::NameString& name_string, BAN::ConstByteSpan aml_data)
	{
		auto resolved_scope = Namespace::root_namespace()->resolve_path(outer_context.scope, name_string);
		if (!resolved_scope.has_value())
			return ParseResult::Failure;

		ParseContext scope_context;
		scope_context.scope = resolved_scope.release_value();
		scope_context.aml_data = aml_data;
		scope_context.method_args = outer_context.method_args;
		while (scope_context.aml_data.size() > 0)
		{
			auto object_result = AML::parse_object(scope_context);
			if (object_result.returned())
			{
				AML_ERROR("Unexpected return from scope {}", scope_context.scope);
				return ParseResult::Failure;
			}
			if (!object_result.success())
				return ParseResult::Failure;
		}

		for (auto& name : scope_context.created_objects)
			MUST(outer_context.created_objects.push_back(BAN::move(name)));

		return ParseResult::Success;
	}

	void AML::Scope::debug_print(int indent) const
	{
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("Scope ");
		name.debug_print();
		AML_DEBUG_PRINTLN(" {");
		for (const auto& [name, object] : objects)
		{
			object->debug_print(indent + 1);
			AML_DEBUG_PRINTLN("");
		}
		AML_DEBUG_PRINT_INDENT(indent);
		AML_DEBUG_PRINT("}");
	}

	bool AML::initialize_scope(BAN::RefPtr<AML::Scope> scope)
	{
#if AML_DEBUG_LEVEL >= 2
		AML_DEBUG_PRINTLN("Initializing {}", scope->scope);
#endif

		bool run_ini = true;
		bool init_children = true;

		auto it = scope->objects.find(NameSeg("_STA"sv));
		if (scope->type != AML::Node::Type::Namespace && it != scope->objects.end() && it->value->type == Node::Type::Method)
		{
			auto* method = static_cast<Method*>(it->value.ptr());
			if (method->arg_count != 0)
			{
				AML_ERROR("Method {}._STA has {} arguments, expected 0", scope->scope, method->arg_count);
				return false;
			}
			BAN::Vector<uint8_t> sync_stack;
			auto result = method->evaluate({}, sync_stack);
			if (!result.has_value())
			{
				AML_ERROR("Failed to evaluate {}._STA, ignoring device", scope->scope);
				return true;
			}
			auto result_value = result.has_value() ? result.value()->as_integer() : BAN::Optional<uint64_t>();
			if (!result_value.has_value())
			{
				AML_ERROR("Failed to evaluate {}._STA, return value could not be resolved to integer", scope->scope);
				AML_ERROR("  Return value: ");
				result.value()->debug_print(0);
				return false;
			}
			run_ini = (result_value.value() & 0x01);
			init_children = run_ini || (result_value.value() & 0x02);
		}

		if (run_ini)
		{
			auto it = scope->objects.find(NameSeg("_STA"sv));
			if (it != scope->objects.end() && it->value->type == Node::Type::Method)
			{
				auto* method = static_cast<Method*>(it->value.ptr());
				if (method->arg_count != 0)
				{
					AML_ERROR("Method {}._INI has {} arguments, expected 0", scope->scope, method->arg_count);
					return false;
				}
				BAN::Vector<uint8_t> sync_stack;
				method->evaluate({}, sync_stack);
			}
		}

		bool success = true;
		if (init_children)
		{
			for (auto& [_, child] : scope->objects)
			{
				if (!child->is_scope())
					continue;
				auto* child_scope = static_cast<Scope*>(child.ptr());
				if (!initialize_scope(child_scope))
					success = false;
			}
		}
		return success;
	}

}
