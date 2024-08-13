#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Register.h>
#include <kernel/ACPI/AML/String.h>

namespace Kernel::ACPI::AML
{

	BAN::RefPtr<AML::Buffer> Register::as_buffer()
	{
		if (value)
			return value->as_buffer();
		return {};
	}

	BAN::RefPtr<AML::Integer> Register::as_integer()
	{
		if (value)
			return value->as_integer();
		return {};
	}

	BAN::RefPtr<AML::String> Register::as_string()
	{
		if (value)
			return value->as_string();
		return {};
	}

}
