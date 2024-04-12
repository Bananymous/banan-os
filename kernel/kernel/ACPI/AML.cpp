#include <BAN/ByteSpan.h>
#include <BAN/Variant.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML.h>

namespace Kernel::ACPI
{

	// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#secondary-system-description-table-ssdt
	// https://uefi.org/htmlspecs/ACPI_Spec_6_4_html/05_ACPI_Software_Programming_Model/ACPI_Software_Programming_Model.html#persistent-system-description-table-psdt
	static bool load_all_unique(AML::Namespace& ns, BAN::StringView signature)
	{
		// Only SSDT and PSDT that have unique OEM Table ID are loaded in order they appear in the RSDT/XSDT
		BAN::Vector<uint64_t> loaded_oem_table_ids;

		for (uint32_t i = 0;; i++)
		{
			auto* header = ACPI::ACPI::get().get_header(signature, i);
			if (!header)
				break;

			bool need_to_parse = true;
			for (uint64_t id : loaded_oem_table_ids)
			{
				if (id == header->oem_table_id)
				{
					need_to_parse = false;
					break;
				}
			}

			if (!need_to_parse)
			{
				dprintln("Skipping {}{} ({} bytes)", signature, i, header->length);
				continue;
			}

			dprintln("Parsing {}{} ({} bytes)", signature, i, header->length);
			if (!ns.parse(*header))
			{
				dwarnln("Failed to parse {}", signature);
				return false;
			}

			MUST(loaded_oem_table_ids.push_back(header->oem_table_id));
		}

		return true;
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

		if (!load_all_unique(*ns, "SSDT"))
			return {};

		if (!load_all_unique(*ns, "PSDT"))
			return {};

#if AML_DEBUG_LEVEL >= 1
		ns->debug_print(0);
		AML_DEBUG_PRINTLN("");
#endif

		dprintln("Parsed ACPI namespace");

		return ns;
	}

}
