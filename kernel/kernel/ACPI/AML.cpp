#include <BAN/ByteSpan.h>
#include <BAN/Variant.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML.h>

namespace Kernel::ACPI
{

	static void load_all(AML::Namespace& ns, BAN::StringView signature)
	{
		for (uint32_t i = 0;; i++)
		{
			auto* header = ACPI::ACPI::get().get_header(signature, i);
			if (!header)
				break;

			dprintln("Parsing {}{} ({} bytes)", signature, i, header->length);
			if (!ns.parse(*header))
			{
				dwarnln("Failed to parse {}", signature);
				continue;
			}
		}
	}

	BAN::RefPtr<AML::Namespace> AML::initialize_namespace()
	{
		auto ns = AML::Namespace::create_root_namespace();

		// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#differentiated-system-description-table-dsdt
		auto* dsdt = ACPI::ACPI::get().get_header("DSDT", 0);
		if (!dsdt)
		{
			dwarnln("Failed to get DSDT");
			return {};
		}
		dprintln("Parsing DSDT ({} bytes)", dsdt->length);
		if (!ns->parse(*dsdt))
		{
			dwarnln("Failed to parse DSDT");
			return {};
		}

		// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#secondary-system-description-table-ssdt
		load_all(*ns, "SSDT");

		// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#persistent-system-description-table-psdt
		load_all(*ns, "PSDT");

#if AML_DEBUG_LEVEL >= 1
		ns->debug_print(0);
		AML_DEBUG_PRINTLN("");
#endif

		dprintln("Parsed ACPI namespace, total of {} nodes created", AML::Node::total_node_count);

		return ns;
	}

}
