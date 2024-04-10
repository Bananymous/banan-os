#include <BAN/ByteSpan.h>
#include <BAN/Variant.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML.h>

namespace Kernel::ACPI
{

	BAN::RefPtr<AML::Namespace> AML::initialize_namespace()
	{
		auto ns = AML::Namespace::create_root_namespace();

		// Parse DSDT
		auto* dsdt = ACPI::ACPI::get().get_header("DSDT", 0);
		if (!dsdt)
		{
			dwarnln("Failed to get DSDT");
			return {};
		}
		if (!ns->parse(*dsdt))
		{
			dwarnln("Failed to parse DSDT");
			return {};
		}

		for (uint32_t i = 0;; i++)
		{
			auto* ssdt = ACPI::ACPI::get().get_header("SSDT", i);
			if (!ssdt)
				break;
			if (!ns->parse(*ssdt))
			{
				dwarnln("Failed to parse SSDT");
				return {};
			}
		}

#if AML_DEBUG_LEVEL >= 1
		ns->debug_print(0);
		AML_DEBUG_PRINTLN("");
#endif

		dprintln("Parsed ACPI namespace");

		return ns;
	}

}
