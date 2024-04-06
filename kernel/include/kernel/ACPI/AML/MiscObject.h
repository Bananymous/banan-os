#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>

namespace Kernel::ACPI::AML
{
	// ACPI Spec 6.4, Section 20.2.6

	// ArgObj := Arg0Op | Arg1Op | Arg2Op | Arg3Op | Arg4Op | Arg5Op | Arg6Op
	struct ArgObj
	{
		enum class Type
		{
			Arg0,
			Arg1,
			Arg2,
			Arg3,
			Arg4,
			Arg5,
			Arg6,
		};
		Type type;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<ArgObj> parse(BAN::ConstByteSpan& span);
	};

	// LocalObj := Local0Op | Local1Op | Local2Op | Local3Op | Local4Op | Local5Op | Local6Op | Local7Op
	struct LocalObj
	{
		enum class Type
		{
			Local0,
			Local1,
			Local2,
			Local3,
			Local4,
			Local5,
			Local6,
			Local7,
		};
		Type type;

		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<LocalObj> parse(BAN::ConstByteSpan& span);
	};

	// DebugObj := DebugOp
	struct DebugObj
	{
		static bool can_parse(BAN::ConstByteSpan span);
		static BAN::Optional<DebugObj> parse(BAN::ConstByteSpan& span);
	};

}
