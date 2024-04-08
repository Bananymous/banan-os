#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/Field.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/Mutex.h>
#include <kernel/ACPI/AML/Names.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/Package.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Processor.h>
#include <kernel/ACPI/AML/Region.h>
#include <kernel/ACPI/AML/String.h>
#include <kernel/ACPI/AML/Utils.h>

namespace Kernel::ACPI
{

	AML::ParseResult AML::ParseResult::Failure = AML::ParseResult(AML::ParseResult::Result::Failure);
	AML::ParseResult AML::ParseResult::Success = AML::ParseResult(AML::ParseResult::Result::Success);

	AML::ParseResult AML::parse_object(AML::ParseContext& context)
	{
		if (context.aml_data.size() < 1)
			return ParseResult::Failure;

		if (static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix)
		{
			if (context.aml_data.size() < 2)
				return ParseResult::Failure;

			switch (static_cast<AML::ExtOp>(context.aml_data[1]))
			{
				case AML::ExtOp::FieldOp:
					return AML::Field::parse(context);
				case AML::ExtOp::IndexFieldOp:
					return AML::IndexField::parse(context);
				case AML::ExtOp::OpRegionOp:
					return AML::OpRegion::parse(context);
				case AML::ExtOp::DeviceOp:
					return AML::Device::parse(context);
				case AML::ExtOp::MutexOp:
					return AML::Mutex::parse(context);
				case AML::ExtOp::ProcessorOp:
					return AML::Processor::parse(context);
				default:
					break;
			}

			AML_TODO("{2H} {2H}", context.aml_data[0], context.aml_data[1]);
			return ParseResult::Failure;
		}

		switch (static_cast<AML::Byte>(context.aml_data[0]))
		{
			case AML::Byte::ZeroOp:
			case AML::Byte::OneOp:
			case AML::Byte::OnesOp:
			case AML::Byte::BytePrefix:
			case AML::Byte::WordPrefix:
			case AML::Byte::DWordPrefix:
			case AML::Byte::QWordPrefix:
				return AML::Integer::parse(context.aml_data);
			case AML::Byte::StringPrefix:
				return AML::String::parse(context);
			case AML::Byte::NameOp:
				return AML::Name::parse(context);
			case AML::Byte::PackageOp:
				return AML::Package::parse(context);
			case AML::Byte::MethodOp:
				return AML::Method::parse(context);
			case AML::Byte::BufferOp:
				return AML::Buffer::parse(context);
			case AML::Byte::ScopeOp:
				return AML::Scope::parse(context);
			default:
				break;
		}

		if (static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::RootChar
			|| static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ParentPrefixChar
			|| static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::NullName
			|| is_lead_name_char(context.aml_data[0]))
		{
			auto name_string = AML::NameString::parse(context.aml_data);
			if (!name_string.has_value())
				return ParseResult::Failure;
			auto aml_object = context.root_namespace->find_object(context.scope.span(), name_string.value());
			if (!aml_object)
			{
				AML_TODO("NameString not found in namespace");
				return ParseResult::Failure;
			}
			return ParseResult(aml_object);
		}

		AML_TODO("{2H}", context.aml_data[0]);
		return ParseResult::Failure;
	}

}
