#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Region.h>
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
			AML_PRINT("Scope '{}' not found in namespace", name_string.value());
			return ParseResult::Success;
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

	static BAN::RefPtr<AML::Integer> evaluate_or_invoke(BAN::RefPtr<AML::Node> object)
	{
		if (object->type != AML::Node::Type::Method)
		{
			auto converted = object->convert(AML::Node::ConvInteger);
			if (!converted)
				return {};
			return static_cast<AML::Integer*>(converted.ptr());
		}

		auto* method = static_cast<AML::Method*>(object.ptr());
		if (method->arg_count != 0)
		{
			AML_ERROR("Method has {} arguments, expected 0", method->arg_count);
			return {};
		}

		auto result = method->invoke();
		if (!result.has_value())
		{
			AML_ERROR("Failed to evaluate method");
			return {};
		}

		auto result_integer = result.value()
			? result.value()->convert(AML::Node::ConvInteger)
			: BAN::RefPtr<AML::Node>();
		return static_cast<AML::Integer*>(result_integer.ptr());
	}

	bool AML::initialize_scope(BAN::RefPtr<AML::Scope> scope)
	{
#if AML_DEBUG_LEVEL >= 2
		AML_DEBUG_PRINTLN("Initializing {}", scope->scope);
#endif

		if (auto reg = Namespace::root_namespace()->find_object(scope->scope, AML::NameString("_REG"_sv), Namespace::FindMode::ForceAbsolute))
		{
			bool embedded_controller = false;
			Namespace::for_each_child(scope->scope,
				[&](const auto&, auto& child)
				{
					if (child->type != AML::Node::Type::OpRegion)
						return;
					auto* region = static_cast<AML::OpRegion*>(child.ptr());
					if (region->region_space == AML::OpRegion::RegionSpace::EmbeddedController)
						embedded_controller = true;
				}
			);
			if (embedded_controller)
			{
				if (reg->type != AML::Node::Type::Method)
				{
					AML_ERROR("Object {}._REG is not a method", scope->scope);
					return false;
				}

				auto* method = static_cast<Method*>(reg.ptr());
				if (method->arg_count != 2)
				{
					AML_ERROR("Method {}._REG has {} arguments, expected 2", scope->scope, method->arg_count);
					return false;
				}

				BAN::RefPtr<AML::Node> embedded_controller = MUST(BAN::RefPtr<AML::Integer>::create(static_cast<uint64_t>(AML::OpRegion::RegionSpace::EmbeddedController)));

				if (!method->invoke(embedded_controller, AML::Integer::Constants::One).has_value())
				{
					AML_ERROR("Failed to evaluate {}._REG(EmbeddedController, 1), ignoring device", scope->scope);
					return false;
				}
			}
		}

		bool run_ini = true;
		bool init_children = true;

		if (auto sta = Namespace::root_namespace()->find_object(scope->scope, AML::NameString("_STA"_sv), Namespace::FindMode::ForceAbsolute))
		{
			auto result = evaluate_or_invoke(sta);
			if (!result)
			{
				AML_ERROR("Failed to evaluate {}._STA, return value could not be resolved to integer", scope->scope);
				return false;
			}

			run_ini = (result->value & 0x01);
			init_children = run_ini || (result->value & 0x02);
		}

		if (run_ini)
		{
			auto ini = Namespace::root_namespace()->find_object(scope->scope, AML::NameString("_INI"_sv), Namespace::FindMode::ForceAbsolute);
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

				auto result = method->invoke();
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
