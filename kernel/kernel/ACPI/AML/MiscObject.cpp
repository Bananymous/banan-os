#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/MiscObject.h>

namespace Kernel::ACPI
{

	// ArgObj

	bool AML::ArgObj::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::Arg0Op:
			case AML::Byte::Arg1Op:
			case AML::Byte::Arg2Op:
			case AML::Byte::Arg3Op:
			case AML::Byte::Arg4Op:
			case AML::Byte::Arg5Op:
			case AML::Byte::Arg6Op:
				return true;
			default:
				return false;
		}
	}

	BAN::Optional<AML::ArgObj> AML::ArgObj::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		uint8_t type = static_cast<uint8_t>(span[0]) - static_cast<uint8_t>(AML::Byte::Arg0Op);
		span = span.slice(1);

		AML_DEBUG_PRINT("Arg{}", type);

		return ArgObj { .type = static_cast<Type>(type) };
	}


	// LocalObj

	bool AML::LocalObj::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::Local0Op:
			case AML::Byte::Local1Op:
			case AML::Byte::Local2Op:
			case AML::Byte::Local3Op:
			case AML::Byte::Local4Op:
			case AML::Byte::Local5Op:
			case AML::Byte::Local6Op:
			case AML::Byte::Local7Op:
				return true;
			default:
				return false;
		}
	}

	BAN::Optional<AML::LocalObj> AML::LocalObj::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		uint8_t type = static_cast<uint8_t>(span[0]) - static_cast<uint8_t>(AML::Byte::Local0Op);
		span = span.slice(1);

		AML_DEBUG_PRINT("Local{}", type);

		return LocalObj { .type = static_cast<Type>(type) };
	}

	// DebugObj

	bool AML::DebugObj::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 2)
			return false;
		if (static_cast<AML::Byte>(span[0]) != AML::Byte::ExtOpPrefix)
			return false;
		if (static_cast<AML::Byte>(span[1]) != AML::Byte::ExtDebugOp)
			return false;
		return true;
	}

	BAN::Optional<AML::DebugObj> AML::DebugObj::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		span = span.slice(2);
		return DebugObj {};
	}

}
