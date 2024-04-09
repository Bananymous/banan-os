#pragma once

#include <BAN/Array.h>
#include <BAN/ByteSpan.h>
#include <BAN/LinkedList.h>
#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/Register.h>

namespace Kernel::ACPI::AML
{

	struct ParseContext
	{
		BAN::ConstByteSpan	aml_data;
		AML::NameString		scope;

		// Used for cleaning up on method exit
		// NOTE: This uses linked list instead of vector because
		//       we don't really need large contiguous memory
		BAN::LinkedList<AML::NameString> created_objects;

		BAN::Array<BAN::RefPtr<Register>, 7> method_args;
		BAN::Array<BAN::RefPtr<Register>, 8> method_locals;
	};

}
