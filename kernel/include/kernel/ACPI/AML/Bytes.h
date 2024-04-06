#pragma once

#include <BAN/Formatter.h>
#include <BAN/Vector.h>
#include <BAN/StringView.h>
#include <kernel/Debug.h>

#define DUMP_AML 0

#if DUMP_AML

#define AML_DEBUG_CONCAT_IMPL(x, y) x##y
#define AML_DEBUG_CONCAT(x, y) AML_DEBUG_CONCAT_IMPL(x, y)
#define AML_DEBUG_INDENT_SCOPE() Kernel::ACPI::AML::Indenter AML_DEBUG_CONCAT(indenter, __COUNTER__)

#define __AML_DEBUG_PRINT_INDENT()													\
	do {																			\
		BAN::Formatter::print(Debug::putchar, "{}:{3} ", __BASE_FILE__, __LINE__);	\
		for (size_t i = 1; i < AML::g_depth; i++)									\
			BAN::Formatter::print(Debug::putchar, "│ ");							\
		if (AML::g_depth > 0)														\
			BAN::Formatter::print(Debug::putchar, "├─");							\
	} while (0)

#define AML_DEBUG_PRINT_FN()													\
	__AML_DEBUG_PRINT_INDENT();													\
	AML_DEBUG_INDENT_SCOPE();													\
	BAN::Formatter::println(Debug::putchar, "{}", AML::get_class_name(__PRETTY_FUNCTION__))

#define AML_DEBUG_PRINT(...)									\
	do {														\
		__AML_DEBUG_PRINT_INDENT();								\
		BAN::Formatter::println(Debug::putchar, __VA_ARGS__);	\
	} while (0)

#else

#define AML_DEBUG_PRINT_FN()
#define AML_DEBUG_PRINT(...)
#define AML_DEBUG_INDENT_SCOPE()
#define __AML_DEBUG_PRINT_INDENT()

#endif

#define AML_DEBUG_TODO(...)										\
	do {														\
		__AML_DEBUG_PRINT_INDENT();								\
		BAN::Formatter::print(Debug::putchar, "\e[33mTODO: ");	\
		BAN::Formatter::print(Debug::putchar, __VA_ARGS__);		\
		BAN::Formatter::println(Debug::putchar, "\e[m");		\
	} while (0)

#define AML_DEBUG_ERROR(...)								\
	do {													\
		__AML_DEBUG_PRINT_INDENT();							\
		BAN::Formatter::print(Debug::putchar, "\e[31m");	\
		BAN::Formatter::print(Debug::putchar, __VA_ARGS__);	\
		BAN::Formatter::println(Debug::putchar, "\e[m");	\
	} while (0)

#define AML_DEBUG_CANNOT_PARSE(TYPE, SPAN)																	\
	do {																									\
		__AML_DEBUG_PRINT_INDENT();																			\
		BAN::Formatter::print(Debug::putchar, "\e[31mCannot parse " TYPE " (span {} bytes", SPAN.size());	\
		if (SPAN.size() > 0)																				\
			BAN::Formatter::print(Debug::putchar, ", {2H}", SPAN[0]);										\
		if (SPAN.size() > 1)																				\
			BAN::Formatter::print(Debug::putchar, " {2H}", SPAN[1]);										\
		BAN::Formatter::println(Debug::putchar, ")\e[m");													\
	} while (0)

namespace Kernel::ACPI::AML
{
	extern size_t g_depth;
	struct Indenter
	{
		Indenter() { g_depth++; }
		~Indenter() { g_depth--; }
	};

	static BAN::StringView get_class_name(BAN::StringView pretty_function)
	{
		return MUST(MUST(pretty_function.split(' '))[2].split(':'))[3];
	}
}

#define GEN_PARSE_CASE_TODO(NAME)	\
	case AML::Byte::NAME:			\
		AML_DEBUG_TODO(#NAME);		\
		return {};

#define AML_TRY_PARSE_PACKAGE(NAME)							\
	auto opt_##NAME = AML::PkgLength::parse_package(span);	\
	if (!opt_##NAME.has_value())							\
		return {};											\
	auto NAME = opt_##NAME.release_value();

#define AML_TRY_PARSE(NAME, TYPE, SPAN)			\
	if (!TYPE::can_parse(SPAN))					\
	{											\
		AML_DEBUG_CANNOT_PARSE(#TYPE, SPAN);	\
		return {};								\
	}											\
	auto NAME = TYPE::parse(SPAN);				\
	if (!NAME.has_value())						\
		return {}

#define AML_TRY_PARSE_IF_CAN(TYPE)							\
	if (TYPE::can_parse(span))								\
	{														\
		if (auto obj = TYPE::parse(span); obj.has_value())	\
			return obj.release_value();						\
		return {};											\
	}



namespace Kernel::ACPI::AML {

	enum class Byte : uint8_t
	{
		ExtOpPrefix = 0x5B,

		// NamePath
		DualNamePrefix	= 0x2E,
		MultiNamePrefix	= 0x2F,

		// NameSpaceModifierObj
		AliasOp	= 0x06,
		NameOp	= 0x08,
		ScopeOp	= 0x10,

		// ConstObj
		ZeroOp			= 0x00,
		OneOp			= 0x01,
		OnesOp			= 0xFF,

		// ComputationalData
		BytePrefix		= 0x0A,
		WordPrefix		= 0x0B,
		DWordPrefix		= 0x0C,
		StringPrefix	= 0x0D,
		QWordPrefix		= 0x0E,
		BufferOp		= 0x11,
		ExtRevisionOp	= 0x30,

		// DataObject
		PackageOp		= 0x12,
		VarPackageOp	= 0x13,

		// NamedObj
		ExternalOp			= 0x15,
		CreateDWordFieldOp	= 0x8A,
		CreateWordFieldOp	= 0x8B,
		CreateByteFieldOp	= 0x8C,
		CreateBitFieldOp	= 0x8D,
		CreateQWordFieldOp	= 0x8F,
		ExtCreateFieldOp	= 0x13,
		ExtOpRegionOp		= 0x80,
		ExtProcessorOp		= 0x83, // deprecated
		ExtPowerResOp		= 0x84,
		ExtThermalZoneOp	= 0x85,
		ExtBankFieldOp		= 0x87,
		ExtDataRegionOp		= 0x88,
		// ... not specified
		MethodOp		= 0x14,
		ExtMutexOp		= 0x01,
		ExtEventOp		= 0x02,
		ExtFieldOp		= 0x81,
		ExtDeviceOp		= 0x82,
		ExtIndexFieldOp	= 0x86,

		// StatementOpcode
		BreakOp			= 0xA5,
		BreakPointOp	= 0xCC,
		ContinueOp		= 0x9F,
		ElseOp			= 0xA1,
		IfOp			= 0xA0,
		NoopOp			= 0xA3,
		NotifyOp		= 0x86,
		ReturnOp		= 0xA4,
		WhileOp			= 0xA2,
		ExtFatalOp		= 0x32,
		ExtReleaseOp	= 0x27,
		ExtResetOp		= 0x26,
		ExtSignalOp		= 0x24,
		ExtSleepOp		= 0x22,
		ExtStallOp		= 0x21,

		// ExpressionOpcode
		//PackageOp			= 0x12,
		//VarPackageOp		= 0x13,
		//BufferOp			= 0x11,
		StoreOp				= 0x70,
		RefOfOp				= 0x71,
		AddOp				= 0x72,
		ConcatOp			= 0x73,
		SubtractOp			= 0x74,
		IncrementOp			= 0x75,
		DecrementOp			= 0x76,
		MultiplyOp			= 0x77,
		DivideOp			= 0x78,
		ShiftLeftOp			= 0x79,
		ShiftRightOp		= 0x7A,
		AndOp				= 0x7B,
		NAndOp				= 0x7C,
		OrOp				= 0x7D,
		NOrOp				= 0x7E,
		XOrOp				= 0x7F,
		NotOp				= 0x80,
		FindSetLeftBitOp	= 0x81,
		FindSetRightBitOp	= 0x82,
		DerefOfOp			= 0x83,
		ConcatResOp			= 0x84,
		ModOp				= 0x85,
		SizeOfOp			= 0x87,
		IndexOp				= 0x88,
		MatchOp				= 0x89,
		ObjectTypeOp		= 0x8E,
		LAndOp				= 0x90,
		LOrOp				= 0x91,
		LNotOp				= 0x92,
		LEqualOp			= 0x93,
		LGreaterOp			= 0x94,
		LLessOp				= 0x95,
		ToBufferOp			= 0x96,
		ToDecimalStringOp	= 0x97,
		ToHexStringOp		= 0x98,
		ToIntegerOp			= 0x99,
		ToStringOp			= 0x9C,
		CopyObjectOp		= 0x9D,
		MidOp				= 0x9E,
		ExtCondRefOfOp		= 0x12,
		ExtLoadTableOp		= 0x1F,
		ExtLoadOp			= 0x20,
		ExtAcquireOp		= 0x23,
		ExtWaitOp			= 0x25,
		ExtFromBCDOp		= 0x28,
		ExtToBCDOp			= 0x29,
		ExtTimerOp			= 0x33,

		// LocalObj
		Local0Op	= 0x60,
		Local1Op	= 0x61,
		Local2Op	= 0x62,
		Local3Op	= 0x63,
		Local4Op	= 0x64,
		Local5Op	= 0x65,
		Local6Op	= 0x66,
		Local7Op	= 0x67,

		// ArgObj
		Arg0Op	= 0x68,
		Arg1Op	= 0x69,
		Arg2Op	= 0x6A,
		Arg3Op	= 0x6B,
		Arg4Op	= 0x6C,
		Arg5Op	= 0x6D,
		Arg6Op	= 0x6E,

		// DebugObj
		ExtDebugOp	= 0x31,
	};

}
