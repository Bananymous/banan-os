#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	BAN::ErrorOr<void> initialize_scope(const Scope& scope)
	{
		bool run_ini = true;
		bool init_children = true;

		if (auto [sta_path, sta_obj] = TRY(Namespace::root_namespace().find_named_object(scope, TRY(NameString::from_string("_STA"_sv)), true)); sta_obj)
		{
			auto sta_result = TRY(evaluate_node(sta_path, sta_obj->node));
			if (sta_result.type != Node::Type::Integer)
			{
				dwarnln("Object {} evaluated to {}", sta_path, sta_result);
				return BAN::Error::from_errno(EFAULT);
			}

			run_ini = (sta_result.as.integer.value & 0x01);
			init_children = run_ini || (sta_result.as.integer.value & 0x02);
		}

		if (run_ini)
		{
			if (auto [ini_path, ini_obj] = TRY(Namespace::root_namespace().find_named_object(scope, TRY(NameString::from_string("_INI"_sv)), true)); ini_obj)
			{
				auto& ini_node = ini_obj->node;

				if (ini_node.type != Node::Type::Method)
				{
					dwarnln("Object {} is not a method", ini_path);
					return BAN::Error::from_errno(EFAULT);
				}

				if (ini_node.as.method.arg_count != 0)
				{
					dwarnln("Method {} takes {} arguments, expected 0", ini_path, ini_node.as.method.arg_count);
					return BAN::Error::from_errno(EFAULT);
				}

				TRY(method_call(ini_path, ini_node, {}));
			}
		}

		BAN::ErrorOr<void> result {};
		if (init_children)
		{
			TRY(Namespace::root_namespace().for_each_child(scope,
				[&result](const Scope& child_path, Reference* child) -> BAN::Iteration
				{
					if (!child->node.is_scope())
						return BAN::Iteration::Continue;
					if (auto ret = initialize_scope(child_path); ret.is_error())
						result = ret.release_error();
					return BAN::Iteration::Continue;
				}
			));
		}

		return result;
	}

}
