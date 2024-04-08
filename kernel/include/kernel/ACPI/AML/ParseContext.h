#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/Namespace.h>

namespace Kernel::ACPI::AML
{

	struct ParseContext
	{
		BAN::ConstByteSpan aml_data;
		BAN::Vector<AML::NameSeg> scope;
		struct Namespace* root_namespace;
	};

}
