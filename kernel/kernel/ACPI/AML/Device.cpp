#include <kernel/ACPI/AML/Device.h>

namespace Kernel::ACPI
{

	bool AML::initialize_device(BAN::RefPtr<AML::NamedObject> device)
	{
		bool run_ini = true;
		bool init_children = true;

		ASSERT(device->type == Node::Type::Device || device->type == Node::Type::Processor);
		ASSERT(device->is_scope());
		auto* scope = static_cast<Scope*>(device.ptr());

		auto it = scope->objects.find(NameSeg("_STA"sv));
		if (it != scope->objects.end() && it->value->type == Node::Type::Method)
		{
			auto* method = static_cast<Method*>(it->value.ptr());
			if (method->arg_count != 0)
			{
				AML_ERROR("Method {}._STA has {} arguments, expected 0", scope, method->arg_count);
				return false;
			}
			BAN::Vector<uint8_t> sync_stack;
			auto result = method->evaluate({}, sync_stack);
			if (!result.has_value())
			{
				AML_ERROR("Failed to evaluate {}._STA", scope);
				return false;
			}
			if (!result.value())
			{
				AML_ERROR("Failed to evaluate {}._STA, return value is null", scope);
				return false;
			}
			auto result_val = result.value()->as_integer();
			if (!result_val.has_value())
			{
				AML_ERROR("Failed to evaluate {}._STA, return value could not be resolved to integer", scope);
				AML_ERROR("  Return value: ");
				result.value()->debug_print(0);
				return false;
			}
			run_ini = (result_val.value() & 0x01);
			init_children = run_ini || (result_val.value() & 0x02);
		}

		if (run_ini)
		{
			auto it = scope->objects.find(NameSeg("_STA"sv));
			if (it != scope->objects.end() && it->value->type == Node::Type::Method)
			{
				auto* method = static_cast<Method*>(it->value.ptr());
				if (method->arg_count != 0)
				{
					AML_ERROR("Method {}._INI has {} arguments, expected 0", scope, method->arg_count);
					return false;
				}
				BAN::Vector<uint8_t> sync_stack;
				method->evaluate({}, sync_stack);
			}
		}

		bool success = true;
		if (init_children)
			for (auto& [_, child] : scope->objects)
				if (child->type == Node::Type::Device || child->type == Node::Type::Processor)
					if (!initialize_device(child))
						success = false;
		return success;
	}

}
