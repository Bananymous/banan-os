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

		BAN::RefPtr<Scope> scope;
		if (auto named_object = context.root_namespace->find_object(context.scope.span(), name_string.value()))
		{
			if (!named_object->is_scope())
			{
				AML_ERROR("Scope name already exists and is not a scope");
				return ParseResult::Failure;
			}
			scope = static_cast<Scope*>(named_object.ptr());
		}
		else
		{
			scope = MUST(BAN::RefPtr<Scope>::create(name_string->path.back()));
			if (!context.root_namespace->add_named_object(context.scope.span(), name_string.value(), scope))
				return ParseResult::Failure;
		}

		return scope->enter_context_and_parse_term_list(context, name_string.value(), scope_pkg.value());
	}

	AML::ParseResult AML::Scope::enter_context_and_parse_term_list(ParseContext& outer_context, const AML::NameString& name_string, BAN::ConstByteSpan aml_data)
	{
		auto scope = outer_context.root_namespace->resolve_path(outer_context.scope.span(), name_string);
		if (!scope.has_value())
			return ParseResult::Failure;

		ParseContext scope_context = outer_context;
		scope_context.scope = scope.release_value();
		scope_context.aml_data = aml_data;
		while (scope_context.aml_data.size() > 0)
		{
			auto object_result = AML::parse_object(scope_context);
			if (!object_result.success())
				return ParseResult::Failure;
		}

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

}
