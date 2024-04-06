#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <BAN/String.h>
#include <BAN/UniqPtr.h>
#include <BAN/Variant.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/MiscObject.h>

namespace Kernel::ACPI::AML
{
	// ACPI Spec 6.4, Section 20.2.2

	// NameSeg := LeadNameChar NameChar NameChar NameChar
	// NameString := ('\' | {'^'}) (NameSeg | (DualNamePrefix NameSeg NameSeg) | (MultiNamePrefix SegCount NameSeg(SegCount)) | 0x00)
	struct NameString
	{
		BAN::String prefix;
		BAN::Vector<BAN::String> path;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<NameString> parse(BAN::ConstByteSpan& span);
	};

	// SimpleName := NameString | ArgObj | LocalObj
	struct SimpleName
	{
		BAN::Variant<NameString, ArgObj, LocalObj> name;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<SimpleName> parse(BAN::ConstByteSpan& span);
	};

	struct ReferenceTypeOpcode;

	// SuperName := SimpleName | DebugObj | ReferenceTypeOpcode
	struct SuperName
	{
		BAN::Variant<SimpleName, DebugObj, BAN::UniqPtr<ReferenceTypeOpcode>> name;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<SuperName> parse(BAN::ConstByteSpan& span);
	};

}
