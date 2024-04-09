#pragma once

#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	struct Device : public AML::Scope
	{
		Device(NameSeg name)
			: Scope(Node::Type::Device, name)
		{}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::DeviceOp);
			context.aml_data = context.aml_data.slice(2);

			auto device_pkg = AML::parse_pkg(context.aml_data);
			if (!device_pkg.has_value())
				return ParseResult::Failure;

			auto name_string = AML::NameString::parse(device_pkg.value());
			if (!name_string.has_value())
				return ParseResult::Failure;

			auto device = MUST(BAN::RefPtr<Device>::create(name_string->path.back()));
			if (!Namespace::root_namespace()->add_named_object(context, name_string.value(), device))
				return ParseResult::Failure;

			return device->enter_context_and_parse_term_list(context, name_string.value(), device_pkg.value());
		}

		void initialize()
		{
			bool run_ini = true;
			bool init_children = true;

			auto _sta = Namespace::root_namespace()->find_object(scope, NameString("_STA"sv));
			if (_sta && _sta->type == Node::Type::Method)
			{
				auto* method = static_cast<Method*>(_sta.ptr());
				if (method->arg_count != 0)
				{
					AML_ERROR("Method {}._STA has {} arguments, expected 0", scope, method->arg_count);
					return;
				}
				auto result = method->evaluate({});
				if (!result.has_value())
				{
					AML_ERROR("Failed to evaluate {}._STA", scope);
					return;
				}
				if (!result.value())
				{
					AML_ERROR("Failed to evaluate {}._STA, return value is null", scope);
					return;
				}
				auto result_val = result.value()->as_integer();
				if (!result_val.has_value())
				{
					AML_ERROR("Failed to evaluate {}._STA, return value could not be resolved to integer", scope);
					AML_ERROR("  Return value: ");
					result.value()->debug_print(0);
					return;
				}
				run_ini = (result_val.value() & 0x01);
				init_children = run_ini || (result_val.value() & 0x02);
			}

			if (run_ini)
			{
				auto _ini = Namespace::root_namespace()->find_object(scope, NameString("_INI"sv));
				if (_ini && _ini->type == Node::Type::Method)
				{
					auto* method = static_cast<Method*>(_ini.ptr());
					if (method->arg_count != 0)
					{
						AML_ERROR("Method {}._INI has {} arguments, expected 0", scope, method->arg_count);
						return;
					}
					method->evaluate({});
				}
			}

			if (init_children)
			{
				for (auto& [_, child] : objects)
				{
					if (child->type == Node::Type::Device)
					{
						auto* device = static_cast<Device*>(child.ptr());
						device->initialize();
					}
				}
			}
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Device ");
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
	};

}
