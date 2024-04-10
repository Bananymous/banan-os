#pragma once

#include <kernel/ACPI/Headers.h>
#include <kernel/ACPI/AML/Namespace.h>

namespace Kernel::ACPI::AML
{

	BAN::RefPtr<AML::Namespace> initialize_namespace();

}
