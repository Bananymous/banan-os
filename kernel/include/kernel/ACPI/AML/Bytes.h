#pragma once

#include <stdint.h>

namespace Kernel::ACPI::AML
{

	enum class Byte
	{
		NullName = 0x00,
		ZeroOp = 0x00,
		OneOp = 0x01,
		// 0x02 - 0x05
		AliasOp = 0x06,
		// 0x07
		NameOp = 0x08,
		// 0x09
		BytePrefix = 0x0A,
		WordPrefix = 0x0B,
		DWordPrefix = 0x0C,
		StringPrefix = 0x0D,
		QWordPrefix = 0x0E,
		// 0x0F
		ScopeOp = 0x10,
		BufferOp = 0x11,
		PackageOp = 0x12,
		VarPackageOp = 0x13,
		MethodOp = 0x14,
		ExternalOp = 0x15,
		// 0x16 - 0x2D
		DualNamePrefix = 0x2E,
		MultiNamePrefix = 0x2F,
		// 0x30 - 0x39 DigitChar
		// 0x3A - 0x40
		// 0x41 - 0x5A NameChar
		ExtOpPrefix = 0x5B,
		RootChar = 0x5C,
		// 0x5D
		ParentPrefixChar = 0x5E,
		// 0x5F NameChar
		Local0 = 0x60,
		Local1 = 0x61,
		Local2 = 0x62,
		Local3 = 0x63,
		Local4 = 0x64,
		Local5 = 0x65,
		Local6 = 0x66,
		Local7 = 0x67,
		Arg0 = 0x68,
		Arg1 = 0x69,
		Arg2 = 0x6A,
		Arg3 = 0x6B,
		Arg4 = 0x6C,
		Arg5 = 0x6D,
		Arg6 = 0x6E,
		// 0x6F
		StoreOp = 0x70,
		RefOfOp = 0x71,
		AddOp = 0x72,
		ConcatOp = 0x73,
		SubtractOp = 0x74,
		IncrementOp = 0x75,
		DecrementOp = 0x76,
		MultiplyOp = 0x77,
		DivideOp = 0x78,
		ShiftLeftOp = 0x79,
		ShiftRightOp = 0x7A,
		AndOp = 0x7B,
		NandOp = 0x7C,
		OrOp = 0x7D,
		NorOp = 0x7E,
		XorOp = 0x7F,
		NotOp = 0x80,
		FindSetLeftBitOp = 0x81,
		FindSetRightBitOp = 0x82,
		DerefOfOp = 0x83,
		ConcatResOp = 0x84,
		ModOp = 0x85,
		NotifyOp = 0x86,
		SizeOfOp = 0x87,
		IndexOp = 0x88,
		MatchOp = 0x89,
		CreateDWordFieldOp = 0x8A,
		CreateWordFieldOp = 0x8B,
		CreateByteFieldOp = 0x8C,
		CreateBitFieldOp = 0x8D,
		ObjectTypeOp = 0x8E,
		CreateQWordFieldOp = 0x8F,
		LAndOp = 0x90,
		LOrOp = 0x91,
		LNotOp = 0x92,
		LEqualOp = 0x93,
		LGreaterOp = 0x94,
		LLessOp = 0x95,
		ToBufferOp = 0x96,
		ToDecimalStringOp = 0x97,
		ToHexStringOp = 0x98,
		ToIntegerOp = 0x99,
		// 0x9A - 0x9B
		ToStringOp = 0x9C,
		CopyObjectOp = 0x9D,
		MidOp = 0x9E,
		ContinueOp = 0x9F,
		IfOp = 0xA0,
		ElseOp = 0xA1,
		WhileOp = 0xA2,
		NoopOp = 0xA3,
		ReturnOp = 0xA4,
		BreakOp = 0xA5,
		// 0xA6 - 0xCB
		BreakPointOp = 0xCC,
		// 0xCD - 0xFE
		OnesOp = 0xFF,
	};

	enum class ExtOp
	{
		MutexOp = 0x01,
		EventOp = 0x02,
		CondRefOfOp = 0x12,
		CreateFieldOp = 0x13,
		LoadTableOp = 0x1F,
		LoadOp = 0x20,
		StallOp = 0x21,
		SleepOp = 0x22,
		AcquireOp = 0x23,
		SignalOp = 0x24,
		WaitOp = 0x25,
		ResetOp = 0x26,
		ReleaseOp = 0x27,
		FromBCDOp = 0x28,
		ToBCDOp = 0x29,
		RevisionOp = 0x30,
		DebugOp = 0x31,
		FatalOp = 0x32,
		TimerOp = 0x33,
		OpRegionOp = 0x80,
		FieldOp = 0x81,
		DeviceOp = 0x82,
		ProcessorOp = 0x83,
		PowerResOp = 0x84,
		ThermalZoneOp = 0x85,
		IndexFieldOp = 0x86,
		BankFieldOp = 0x87,
		DataRegionOp = 0x88,
	};

	inline constexpr bool is_digit_char(uint8_t ch)
	{
		return '0' <= ch && ch <= '9';
	}

	inline constexpr bool is_lead_name_char(uint8_t ch)
	{
		return ('A' <= ch && ch <= 'Z') || ch == '_';
	}

	inline constexpr bool is_name_char(uint8_t ch)
	{
		return is_lead_name_char(ch) || is_digit_char(ch);
	}

	inline constexpr bool is_name_string_start(uint8_t ch)
	{
		if (is_lead_name_char(ch))
			return true;
		switch (static_cast<AML::Byte>(ch))
		{
			case AML::Byte::RootChar:
			case AML::Byte::ParentPrefixChar:
			case AML::Byte::MultiNamePrefix:
			case AML::Byte::DualNamePrefix:
				return true;
			default:
				return false;
		}
	}

}
