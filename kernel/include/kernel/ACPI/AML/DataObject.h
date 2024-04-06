#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <BAN/String.h>
#include <BAN/UniqPtr.h>
#include <BAN/Variant.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/NameObject.h>

namespace Kernel::ACPI::AML
{
	// ACPI Spec 6.4, Section 20.2.3

	// Integer := ByteConst | WordConst | DWordConst | QWordConst
	// Not actually defined in the spec...
	struct Integer
	{
		uint64_t value;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<Integer> parse(BAN::ConstByteSpan& span);
	};

	// Buffer := BufferOp PkgLength BufferSize ByteList
	struct Buffer
	{
		Integer buffer_size;
		BAN::Vector<uint8_t> data;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<Buffer> parse(BAN::ConstByteSpan& span);
	};

	// ComputationalData := Integer | String | ConstObj | RevisionOp | DefBuffer
	struct ComputationalData
	{
		struct String
		{
			BAN::String value;
		};
		struct ConstObj
		{
			enum class Type
			{
				Zero,
				One,
				Ones
			};
			Type type;
		};
		struct RevisionOp {};

		BAN::Variant<Integer, String, ConstObj, RevisionOp, Buffer> data;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<ComputationalData> parse(BAN::ConstByteSpan& span);
	};

	struct DataRefObject;

	// PackageElement := DataRefObject | NameString
	using PackageElement = BAN::Variant<BAN::UniqPtr<DataRefObject>, NameString>;

	// DefPackage := PackageOp PkgLength NumElements PackageElementList
	struct Package
	{
		BAN::Vector<PackageElement> elements;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<Package> parse(BAN::ConstByteSpan& span);
	};

	// DefVarPackage := VarPackageOp PkgLength VarNumElements PackageElementList
	struct VarPackage
	{
		BAN::Vector<PackageElement> elements;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<VarPackage> parse(BAN::ConstByteSpan& span);
	};

	// DataObject := ComputationalData | DefPackage | DefVarPackage
	struct DataObject
	{
		BAN::Variant<ComputationalData, Package, VarPackage> data;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<DataObject> parse(BAN::ConstByteSpan& span);
	};

	// DataRefObject := DataObject | ObjectReference
	struct DataRefObject
	{
		BAN::Variant<DataObject, Integer> object;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<DataRefObject> parse(BAN::ConstByteSpan& span);
	};

}
