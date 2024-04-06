#pragma once

#include <kernel/ACPI/Headers.h>

namespace Kernel::ACPI
{

	class AMLParser
	{
	public:
		~AMLParser();

		static AMLParser parse_table(const SDTHeader& header);

	private:
		AMLParser();
	};

}
