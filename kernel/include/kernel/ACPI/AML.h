#pragma once

#include <kernel/ACPI/Headers.h>
#include <kernel/ACPI/AML/Namespace.h>

namespace Kernel::ACPI
{

	class AMLParser
	{
	public:
		~AMLParser();

		static BAN::RefPtr<AML::Namespace> parse_table(const SDTHeader& header);

	private:
		AMLParser();
	};

}
