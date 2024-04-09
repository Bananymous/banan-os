#include <BAN/ByteSpan.h>
#include <BAN/Variant.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML.h>

namespace Kernel::ACPI
{

	BAN::RefPtr<AML::Namespace> AML::initialize_namespace(const SDTHeader& header)
	{
		dprintln("Parsing {}, {} bytes of AML", header, header.length);

		auto aml_raw = BAN::ConstByteSpan { reinterpret_cast<const uint8_t*>(&header), header.length };
		aml_raw = aml_raw.slice(sizeof(header));

		auto ns = AML::Namespace::parse(aml_raw);
		if (!ns)
		{
			dwarnln("Failed to parse ACPI namespace");
			return {};
		}

#if AML_DEBUG_LEVEL >= 1
		ns->debug_print(0);
		AML_DEBUG_PRINTLN("");
#endif

		dprintln("Parsed ACPI namespace");

		return ns;
	}

}
