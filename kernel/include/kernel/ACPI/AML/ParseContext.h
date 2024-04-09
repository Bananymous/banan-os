#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/LinkedList.h>
#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/Namespace.h>

namespace Kernel::ACPI::AML
{

	struct ParseContext
	{
		BAN::ConstByteSpan	aml_data;
		AML::NameString		scope;
		AML::Namespace*		root_namespace;

		// Used for cleaning up on method exit
		// NOTE: This uses linked list instead of vector because
		//       we don't really need large contiguous memory
		BAN::LinkedList<AML::NameString> created_objects;
	};

}
