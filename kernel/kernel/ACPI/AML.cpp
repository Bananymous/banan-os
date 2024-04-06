#include <BAN/ByteSpan.h>
#include <BAN/Variant.h>
#include <kernel/ACPI/ACPI.h>
#include <kernel/ACPI/AML.h>

#include <kernel/ACPI/AML/TermObject.h>

namespace Kernel::ACPI::AML { size_t g_depth = 0; }

namespace Kernel::ACPI
{

	AMLParser::AMLParser() = default;
	AMLParser::~AMLParser() = default;

	AMLParser AMLParser::parse_table(const SDTHeader& header)
	{
		dprintln("Parsing {}, {} bytes of AML", header, header.length - sizeof(header));

		auto aml_raw = BAN::ConstByteSpan { reinterpret_cast<const uint8_t*>(&header), header.length };
		aml_raw = aml_raw.slice(sizeof(header));

		if (!AML::TermList::can_parse(aml_raw))
			dwarnln("Can not AML term_list");
		else
		{
			auto term_list = AML::TermList::parse(aml_raw);
			if (!term_list.has_value())
				dwarnln("Failed to parse AML term_list");
		}

		return {};
	}

}
