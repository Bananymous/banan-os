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

		auto named_object = Namespace::root_namespace()->find_object(context.scope, name_string.value(), Namespace::FindMode::Normal);
		if (!named_object)
		{
			AML_ERROR("Scope '{}' not found in namespace", name_string.value());
			return ParseResult::Failure;
		}
		if (!named_object->is_scope())
		{
			AML_ERROR("Scope '{}' does not name a namespace", name_string.value());
			return ParseResult::Failure;
		}

		auto* scope = static_cast<Scope*>(named_object.ptr());
		return scope->enter_context_and_parse_term_list(context, name_string.value(), scope_pkg.value());
	}

	AML::ParseResult AML::Scope::enter_context_and_parse_term_list(ParseContext& outer_context, const AML::NameString& name_string, BAN::ConstByteSpan aml_data)
	{
		auto resolved_scope = Namespace::root_namespace()->resolve_path(outer_context.scope, name_string, Namespace::FindMode::Normal);
		if (!resolved_scope.has_value())
			return ParseResult::Failure;

		ParseContext scope_context;
		scope_context.scope = AML::NameString(resolved_scope.release_value());
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

	static BAN::Optional<uint64_t> evaluate_or_invoke(BAN::RefPtr<AML::Node> object)
	{
		if (object->type != AML::Node::Type::Method)
			return object->as_integer();

		auto* method = static_cast<AML::Method*>(object.ptr());
		if (method->arg_count != 0)
		{
			AML_ERROR("Method has {} arguments, expected 0", method->arg_count);
			return {};
		}

		BAN::Vector<uint8_t> sync_stack;
		auto result = method->evaluate({}, sync_stack);
		if (!result.has_value())
		{
			AML_ERROR("Failed to evaluate method");
			return {};
		}

		return result.value() ? result.value()->as_integer() : BAN::Optional<uint64_t>();
	}

	bool AML::initialize_scope(BAN::RefPtr<AML::Scope> scope)
	{
#if AML_DEBUG_LEVEL >= 2
		AML_DEBUG_PRINTLN("Initializing {}", scope->scope);
#endif

		bool run_ini = true;
		bool init_children = true;

		if (auto sta = Namespace::root_namespace()->find_object(scope->scope, AML::NameString("_STA"sv), Namespace::FindMode::ForceAbsolute))
		{
			auto result = evaluate_or_invoke(sta);
			if (!result.has_value())
			{
				AML_ERROR("Failed to evaluate {}._STA, return value could not be resolved to integer", scope->scope);
				return false;
			}

			run_ini = (result.value() & 0x01);
			init_children = run_ini || (result.value() & 0x02);
		}

		if (run_ini)
		{
			auto ini = Namespace::root_namespace()->find_object(scope->scope, AML::NameString("_INI"sv), Namespace::FindMode::ForceAbsolute);
			if (ini)
			{
				if (ini->type != AML::Node::Type::Method)
				{
					AML_ERROR("Object {}._INI is not a method", scope->scope);
					return false;
				}

				auto* method = static_cast<Method*>(ini.ptr());
				if (method->arg_count != 0)
				{
					AML_ERROR("Method {}._INI has {} arguments, expected 0", scope->scope, method->arg_count);
					return false;
				}

				BAN::Vector<uint8_t> sync_stack;
				auto result = method->evaluate({}, sync_stack);
				if (!result.has_value())
				{
					AML_ERROR("Failed to evaluate {}._INI, ignoring device", scope->scope);
					return true;
				}
			}
		}

		bool success = true;
		if (init_children)
		{
			Namespace::root_namespace()->for_each_child(scope->scope,
				[&](const auto&, auto& child)
				{
					if (!child->is_scope())
						return;
					auto* child_scope = static_cast<Scope*>(child.ptr());
					if (!initialize_scope(child_scope))
						success = false;
				}
			);
		}
		return success;
	}

}
