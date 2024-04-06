#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <BAN/UniqPtr.h>
#include <BAN/Variant.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/DataObject.h>
#include <kernel/ACPI/AML/MiscObject.h>
#include <kernel/ACPI/AML/NameObject.h>

namespace Kernel::ACPI::AML
{
	// ACPI Spec 6.4, Section 20.2.5

	struct TermObj;
	struct TermArg;

	// TermList := Nothing | TermObj TermList
	struct TermList
	{
		BAN::Vector<TermObj> terms;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<TermList> parse(BAN::ConstByteSpan&);
	};

	// MethodInvocation := NameString TermArgList
	struct MethodInvocation
	{
		NameString name;
		BAN::Vector<TermArg> term_args;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<MethodInvocation> parse(BAN::ConstByteSpan&);
	};

	// ExpressionOpcode := DefAcquire | DefAdd | DefAnd | DefBuffer | DefConcat | DefConcatRes | DefCondRefOf
	//                   | DefCopyObject | DefDecrement | DefDerefOf | DefDivide | DefFindSetLeftBit
	//                   | DefFindSetRightBit | DefFromBCD | DefIncrement | DefIndex | DefLAnd | DefLEqual
	//                   | DefLGreater | DefLGreaterEqual | DefLLess | DefLLessEqual | DefMid | DefLNot
	//                   | DefLNotEqual | DefLoadTable | DefLOr | DefMatch | DefMod | DefMultiply | DefNAnd
	//                   | DefNOr | DefNot | DefObjectType | DefOr | DefPackage | DefVarPackage | DefRefOf
	//                   | DefShiftLeft | DefShiftRight | DefSizeOf | DefStore | DefSubtract | DefTimer
	//                   | DefToBCD | DefToBuffer | DefToDecimalString | DefToHexString | DefToInteger
	//                   | DefToString | DefWait | DefXOr | MethodInvocation
	struct ExpressionOpcode
	{
		struct UnaryOp
		{
			enum class Type
			{
				Decrement,
				Increment,
				RefOf,
				SizeOf,
			};
			Type type;
			SuperName object;
		};

		struct BinaryOp
		{
			enum class Type
			{
				Add,
				And,
				Multiply,
				NAnd,
				NOr,
				Or,
				Subtract,
				XOr,
				ShiftLeft,
				ShiftRight,
			};
			Type type;
			BAN::UniqPtr<TermArg> source1;
			BAN::UniqPtr<TermArg> source2;
			SuperName target;
		};

		struct LogicalBinaryOp
		{
			enum class Type
			{
				And,
				Equal,
				Greater,
				Less,
				Or,
				// GreaterEqual, LessEqual, NotEqual handled by Not + LogicalBinaryOp
			};
			Type type;
			BAN::UniqPtr<TermArg> operand1;
			BAN::UniqPtr<TermArg> operand2;
		};

#define GEN_OPCODE_STRUCT_OPERAND_TARGET(NAME) struct NAME { BAN::UniqPtr<TermArg> operand; SuperName target; }
		GEN_OPCODE_STRUCT_OPERAND_TARGET(ToBuffer);
		GEN_OPCODE_STRUCT_OPERAND_TARGET(ToDecimalString);
		GEN_OPCODE_STRUCT_OPERAND_TARGET(ToHexString);
		GEN_OPCODE_STRUCT_OPERAND_TARGET(ToInteger);
#undef GEN_OPCODE_STRUCT_OPERAND_TARGET

		struct Acquire
		{
			SuperName mutex;
			uint16_t timeout;
		};
		struct Store
		{
			BAN::UniqPtr<TermArg> source;
			SuperName target;
		};

		BAN::Variant<
			UnaryOp, BinaryOp, LogicalBinaryOp,
			ToBuffer, ToDecimalString, ToHexString, ToInteger,
			Acquire, Buffer, Package, VarPackage, Store, MethodInvocation
		> opcode;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<ExpressionOpcode> parse(BAN::ConstByteSpan&);
	};

	// TermArg := ExpressionOpcode | DataObject | ArgObj | LocalObj
	struct TermArg
	{
		BAN::Variant<ExpressionOpcode, DataObject, ArgObj, LocalObj> arg;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<TermArg> parse(BAN::ConstByteSpan&);
	};

	// NameSpaceModifierObj := DefAlias | DefName | DefScope
	struct NameSpaceModifierObj
	{
		struct Alias {};
		struct Name
		{
			NameString name;
			DataRefObject object;
		};
		struct Scope
		{
			NameString name;
			TermList term_list;
		};
		BAN::Variant<Alias, Name, Scope> modifier;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<NameSpaceModifierObj> parse(BAN::ConstByteSpan&);
	};

	// NamedObj := DefBankField | DefCreateBitField | DefCreateByteField | DefCreateDWordField | DefCreateField
	//           | DefCreateQWordField | DefCreateWordField | DefDataRegion | DefExternal | DefOpRegion
	//           | DefProcessor(deprecated) | DefPowerRes | DefThermalZone
	// Spec does not specify any of DefDevice, DefEvent, DefField, DefIndexField, DefMethod, DefMutex as options
	struct NamedObj
	{
		struct CreateSizedField
		{
			enum class Type
			{
				Bit,
				Byte,
				Word,
				DWord,
				QWord,
			};
			Type type;
			TermArg buffer;
			TermArg index;
			NameString name;
		};

		struct BankField {};
		struct CreateField {};
		struct DataRegion {};
		struct External {};
		struct OpRegion
		{
			enum class RegionSpace : uint8_t
			{
				SystemMemory = 0x00,
				SystemIO = 0x01,
				PCIConfigSpace = 0x02,
				EmbeddedController = 0x03,
				SMBus = 0x04,
				SystemCMOS = 0x05,
				PCIBarTarget = 0x06,
				IPMI = 0x07,
				GeneralPurposeIO = 0x08,
				ResourceDescriptor = 0x09,
				PCC = 0x0A,
			};

			NameString name;
			RegionSpace region_space;
			TermArg region_offset;
			TermArg region_length;
		};
		struct Processor
		{
			NameString name;
			uint8_t processor_id;
			uint32_t p_blk_address;
			uint8_t p_blk_length;
			TermList term_list;
		};
		struct PowerRes {};
		struct ThermalZone {};
		struct Device
		{
			NameString name;
			TermList term_list;
		};
		struct Event {};
		struct Field
		{
			NameString name;
			// field flags
			// field list
		};
		struct IndexField {};
		struct Method
		{
			NameString name;
			uint8_t argument_count;
			bool serialized;
			uint8_t sync_level;
			TermList term_list;
		};
		struct Mutex
		{
			NameString name;
			uint8_t sync_level;
		};

		BAN::Variant<BankField, CreateSizedField, CreateField, DataRegion,
			External, OpRegion, PowerRes, Processor, ThermalZone, Device,
			Event, Field, IndexField, Method, Mutex
		> object;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<NamedObj> parse(BAN::ConstByteSpan&);
	};

	// Object := NameSpaceModifierObj | NamedObj
	struct Object
	{
		BAN::Variant<NameSpaceModifierObj, NamedObj> object;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<Object> parse(BAN::ConstByteSpan&);
	};

	// StatementOpcode := DefBreak | DefBreakPoint | DefContinue | DefFatal | DefIfElse | DefNoop | DefNotify
	//                  | DefRelease | DefReset | DefReturn | DefSignal | DefSleep | DefStall | DefWhile
	struct StatementOpcode
	{
		struct IfElse
		{
			TermArg predicate;
			TermList true_list;
			TermList false_list;
		};
		struct Notify
		{
			SuperName object;
			TermArg value;
		};
		struct Release
		{
			SuperName mutex;
		};
		struct Return
		{
			// TODO: Is argument actually optional?
			//       This is not specified in the spec but it seems like it should be
			BAN::Optional<DataRefObject> arg;
		};

		BAN::Variant<IfElse, Notify, Release, Return> opcode;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<StatementOpcode> parse(BAN::ConstByteSpan&);
	};

	// TermObj := Object | StatementOpcode | ExpressionOpcode
	struct TermObj
	{
		BAN::Variant<Object, StatementOpcode, ExpressionOpcode> term;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<TermObj> parse(BAN::ConstByteSpan&);
	};

	// ReferenceTypeOpcode := DefRefOf | DefDerefOf | DefIndex | UserTermObj
	struct ReferenceTypeOpcode
	{
		struct RefOf
		{
			SuperName target;
		};
		struct DerefOf
		{
			TermArg source;
		};
		struct Index
		{
			TermArg source;
			TermArg index;
			SuperName destination;
		};
		struct UserTermObj
		{
			MethodInvocation method;
		};

		BAN::Variant<RefOf, DerefOf, Index, UserTermObj> opcode;

		static bool can_parse(BAN::ConstByteSpan);
		static BAN::Optional<ReferenceTypeOpcode> parse(BAN::ConstByteSpan&);
	};

}
