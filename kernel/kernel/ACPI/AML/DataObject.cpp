#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/DataObject.h>
#include <kernel/ACPI/AML/PackageLength.h>

namespace Kernel::ACPI
{

	// Integer

	bool AML::Integer::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::BytePrefix:
			case AML::Byte::WordPrefix:
			case AML::Byte::DWordPrefix:
			case AML::Byte::QWordPrefix:
				return true;
			default:
				return false;
		}
	}

	BAN::Optional<AML::Integer> AML::Integer::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		switch (static_cast<AML::Byte>(span[0]))
		{
#define AML_PARSE_INTEGER_CASE(TYPE, BYTES)											\
			case AML::Byte::TYPE##Prefix:											\
			{																		\
				span = span.slice(1);												\
				if (span.size() < BYTES)											\
				{																	\
					AML_DEBUG_CANNOT_PARSE(#TYPE, span);							\
					return {};														\
				}																	\
				uint64_t value = 0;													\
				for (size_t i = 0; i < BYTES; i++)									\
					value |= static_cast<uint64_t>(span[i]) << (i * 8);				\
				AML_DEBUG_PRINT("0x{H}", value);									\
				span = span.slice(BYTES);											\
				return Integer { .value = value };									\
			}
			AML_PARSE_INTEGER_CASE(Byte,  1)
			AML_PARSE_INTEGER_CASE(Word,  2)
			AML_PARSE_INTEGER_CASE(DWord, 4)
			AML_PARSE_INTEGER_CASE(QWord, 8)
#undef AML_PARSE_INTEGER_CASE
			default:
				ASSERT_NOT_REACHED();
		}
	}


	// Buffer

	bool AML::Buffer::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		if (static_cast<AML::Byte>(span[0]) == AML::Byte::BufferOp)
			return true;
		return false;
	}

	BAN::Optional<AML::Buffer> AML::Buffer::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		span = span.slice(1);

		AML_TRY_PARSE_PACKAGE(buffer_span);

		AML_TRY_PARSE(buffer_size, AML::Integer, buffer_span);

		BAN::Vector<uint8_t> data;
		MUST(data.resize(BAN::Math::max(buffer_size->value, buffer_span.size())));
		for (size_t i = 0; i < buffer_span.size(); i++)
			data[i] = buffer_span[i];

		return Buffer { .buffer_size = buffer_size.release_value(), .data = BAN::move(data) };
	}


	// ComputationalData

	bool AML::ComputationalData::can_parse(BAN::ConstByteSpan span)
	{
		if (span.size() < 1)
			return false;
		if (static_cast<AML::Byte>(span[0]) == AML::Byte::ExtOpPrefix)
		{
			if (span.size() < 2)
				return false;
			switch (static_cast<AML::Byte>(span[1]))
			{
				case AML::Byte::ExtRevisionOp:
					return true;
				default:
					return false;
			}
		}
		switch (static_cast<AML::Byte>(span[0]))
		{
			case AML::Byte::ZeroOp:
			case AML::Byte::OneOp:
			case AML::Byte::OnesOp:
			case AML::Byte::BytePrefix:
			case AML::Byte::WordPrefix:
			case AML::Byte::DWordPrefix:
			case AML::Byte::StringPrefix:
			case AML::Byte::QWordPrefix:
			case AML::Byte::BufferOp:
				return true;
			default:
				return false;
		}
	}

	BAN::Optional<AML::ComputationalData> AML::ComputationalData::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));

		AML_TRY_PARSE_IF_CAN(AML::Buffer);
		AML_TRY_PARSE_IF_CAN(AML::Integer);

		switch (static_cast<AML::Byte>(span[0]))
		{
#define AML_PARSE_CONST(TYPE)										\
			case AML::Byte::TYPE##Op:								\
			{														\
				span = span.slice(1);								\
				AML_DEBUG_PRINT("{}", #TYPE);						\
				return ConstObj { .type = ConstObj::Type::TYPE };	\
			}
			AML_PARSE_CONST(Zero);
			AML_PARSE_CONST(One);
			AML_PARSE_CONST(Ones);
#undef AML_PARSE_CONST
			case AML::Byte::StringPrefix:
			{
				span = span.slice(1);

				BAN::String value;
				while (span.size() > 0)
				{
					if (span[0] == 0x00 || span[0] > 0x7F)
						break;
					MUST(value.push_back(span[0]));
					span = span.slice(1);
				}

				if (span.size() == 0 || span[0] != 0x00)
					return {};
				span = span.slice(1);

				AML_DEBUG_PRINT("\"{}\"", value);
				return String { .value = BAN::move(value) };
			}
			GEN_PARSE_CASE_TODO(BufferOp)
			default:
				ASSERT_NOT_REACHED();
		}

		ASSERT_NOT_REACHED();
	}


#define AML_GEN_PACKAGE(NAME)																	\
	bool AML::NAME::can_parse(BAN::ConstByteSpan span)											\
	{																							\
		if (span.size() < 1)																	\
			return false;																		\
		if (static_cast<AML::Byte>(span[0]) == AML::Byte::NAME##Op)								\
			return true;																		\
		return false;																			\
	}																							\
																								\
	BAN::Optional<AML::NAME> AML::NAME::parse(BAN::ConstByteSpan& span)							\
	{																							\
		AML_DEBUG_PRINT_FN();																	\
		ASSERT(can_parse(span));																\
																								\
		span = span.slice(1);																	\
																								\
		AML_TRY_PARSE_PACKAGE(package_span);													\
																								\
		uint8_t count = package_span[0];														\
		package_span = package_span.slice(1);													\
																								\
		AML_DEBUG_PRINT("Count: {}", count);													\
																								\
		BAN::Vector<PackageElement> elements;													\
		for (uint8_t i = 0; package_span.size() > 0 && i < count; i++)							\
		{																						\
			if (DataRefObject::can_parse(package_span))											\
			{																					\
				AML_TRY_PARSE(element, DataRefObject, package_span);							\
				MUST(elements.push_back(PackageElement {										\
					MUST(BAN::UniqPtr<DataRefObject>::create(element.release_value()))			\
				}));																			\
			}																					\
			else if (NameString::can_parse(package_span))										\
			{																					\
				AML_TRY_PARSE(element, NameString, package_span);								\
				MUST(elements.push_back(PackageElement {										\
					element.release_value()														\
				}));																			\
			}																					\
			else																				\
			{																					\
				AML_DEBUG_CANNOT_PARSE("PackageElement", package_span);							\
				return {};																		\
			}																					\
		}																						\
																								\
		while (elements.size() < count)															\
			MUST(elements.push_back(PackageElement { Uninitialized {} }));						\
																								\
		return NAME { .elements = BAN::move(elements) };										\
	}

	AML_GEN_PACKAGE(Package)
	AML_GEN_PACKAGE(VarPackage)
#undef AML_GEN_PACKAGE

	// DataObject

	bool AML::DataObject::can_parse(BAN::ConstByteSpan span)
	{
		if (ComputationalData::can_parse(span))
			return true;
		if (Package::can_parse(span))
			return true;
		if (VarPackage::can_parse(span))
			return true;
		return false;
	}

	BAN::Optional<AML::DataObject> AML::DataObject::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));
		AML_TRY_PARSE_IF_CAN(ComputationalData);
		AML_TRY_PARSE_IF_CAN(Package);
		AML_TRY_PARSE_IF_CAN(VarPackage);
		ASSERT_NOT_REACHED();
	}


	// DataRefObject

	bool AML::DataRefObject::can_parse(BAN::ConstByteSpan span)
	{
		if (DataObject::can_parse(span))
			return true;
		if (Integer::can_parse(span))
			return true;
		return false;
	}

	BAN::Optional<AML::DataRefObject> AML::DataRefObject::parse(BAN::ConstByteSpan& span)
	{
		AML_DEBUG_PRINT_FN();
		ASSERT(can_parse(span));
		AML_TRY_PARSE_IF_CAN(DataObject);
		AML_TRY_PARSE_IF_CAN(Integer);
		ASSERT_NOT_REACHED();
	}

}
