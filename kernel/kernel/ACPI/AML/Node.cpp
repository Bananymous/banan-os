#include <kernel/ACPI/AML/Alias.h>
#include <kernel/ACPI/AML/Buffer.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Conversion.h>
#include <kernel/ACPI/AML/CopyObject.h>
#include <kernel/ACPI/AML/Device.h>
#include <kernel/ACPI/AML/Expression.h>
#include <kernel/ACPI/AML/Event.h>
#include <kernel/ACPI/AML/Field.h>
#include <kernel/ACPI/AML/IfElse.h>
#include <kernel/ACPI/AML/Index.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/Method.h>
#include <kernel/ACPI/AML/Mutex.h>
#include <kernel/ACPI/AML/Names.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/Notify.h>
#include <kernel/ACPI/AML/ObjectType.h>
#include <kernel/ACPI/AML/Package.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/PowerResource.h>
#include <kernel/ACPI/AML/Processor.h>
#include <kernel/ACPI/AML/Reference.h>
#include <kernel/ACPI/AML/Region.h>
#include <kernel/ACPI/AML/SizeOf.h>
#include <kernel/ACPI/AML/Sleep.h>
#include <kernel/ACPI/AML/Store.h>
#include <kernel/ACPI/AML/String.h>
#include <kernel/ACPI/AML/ThermalZone.h>
#include <kernel/ACPI/AML/Utils.h>
#include <kernel/ACPI/AML/While.h>

namespace Kernel::ACPI
{

	AML::ParseResult AML::ParseResult::Failure = AML::ParseResult(AML::ParseResult::Result::Failure);
	AML::ParseResult AML::ParseResult::Success = AML::ParseResult(AML::ParseResult::Result::Success);

	uint64_t AML::Node::total_node_count = 0;

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
				case AML::ExtOp::BankFieldOp:
					return AML::BankField::parse(context);
				case AML::ExtOp::CreateFieldOp:
					return AML::BufferField::parse(context);
				case AML::ExtOp::OpRegionOp:
					return AML::OpRegion::parse(context);
				case AML::ExtOp::DeviceOp:
					return AML::Device::parse(context);
				case AML::ExtOp::EventOp:
				case AML::ExtOp::ResetOp:
				case AML::ExtOp::SignalOp:
				case AML::ExtOp::WaitOp:
					return AML::Event::parse(context);
				case AML::ExtOp::MutexOp:
				case AML::ExtOp::AcquireOp:
				case AML::ExtOp::ReleaseOp:
					return AML::Mutex::parse(context);
				case AML::ExtOp::ProcessorOp:
					return AML::Processor::parse(context);
				case AML::ExtOp::PowerResOp:
					return AML::PowerResource::parse(context);
				case AML::ExtOp::ThermalZoneOp:
					return AML::ThermalZone::parse(context);
				case AML::ExtOp::CondRefOfOp:
					return AML::Reference::parse(context);
				case AML::ExtOp::SleepOp:
					return AML::Sleep::parse(context);
				case AML::ExtOp::DebugOp:
					context.aml_data = context.aml_data.slice(2);
					return ParseResult(AML::Namespace::debug_node);
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
			case AML::Byte::Arg0:
			case AML::Byte::Arg1:
			case AML::Byte::Arg2:
			case AML::Byte::Arg3:
			case AML::Byte::Arg4:
			case AML::Byte::Arg5:
			case AML::Byte::Arg6:
			{
				uint8_t index = context.aml_data[0] - static_cast<uint8_t>(AML::Byte::Arg0);
				context.aml_data = context.aml_data.slice(1);
				return ParseResult(context.method_args[index]);
			}
			case AML::Byte::Local0:
			case AML::Byte::Local1:
			case AML::Byte::Local2:
			case AML::Byte::Local3:
			case AML::Byte::Local4:
			case AML::Byte::Local5:
			case AML::Byte::Local6:
			case AML::Byte::Local7:
			{
				uint8_t index = context.aml_data[0] - static_cast<uint8_t>(AML::Byte::Local0);
				context.aml_data = context.aml_data.slice(1);
				return ParseResult(context.method_locals[index]);
			}
			case AML::Byte::AddOp:
			case AML::Byte::AndOp:
			case AML::Byte::DecrementOp:
			case AML::Byte::DivideOp:
			case AML::Byte::IncrementOp:
			case AML::Byte::LAndOp:
			case AML::Byte::LEqualOp:
			case AML::Byte::LGreaterOp:
			case AML::Byte::LLessOp:
			case AML::Byte::LNotOp:
			case AML::Byte::LOrOp:
			case AML::Byte::ModOp:
			case AML::Byte::MultiplyOp:
			case AML::Byte::NandOp:
			case AML::Byte::NorOp:
			case AML::Byte::NotOp:
			case AML::Byte::OrOp:
			case AML::Byte::ShiftLeftOp:
			case AML::Byte::ShiftRightOp:
			case AML::Byte::SubtractOp:
			case AML::Byte::XorOp:
				return AML::Expression::parse(context);
			case AML::Byte::ToBufferOp:
			case AML::Byte::ToHexStringOp:
			case AML::Byte::ToIntegerOp:
			case AML::Byte::ToStringOp:
				return AML::Conversion::parse(context);
			case AML::Byte::CreateBitFieldOp:
			case AML::Byte::CreateByteFieldOp:
			case AML::Byte::CreateWordFieldOp:
			case AML::Byte::CreateDWordFieldOp:
			case AML::Byte::CreateQWordFieldOp:
				return AML::BufferField::parse(context);
			case AML::Byte::AliasOp:
				return AML::Alias::parse(context);
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
			case AML::Byte::IfOp:
				return AML::IfElse::parse(context);
			case AML::Byte::BreakOp:
			case AML::Byte::ContinueOp:
			case AML::Byte::WhileOp:
				return AML::While::parse(context);
			case AML::Byte::StoreOp:
				return AML::Store::parse(context);
			case AML::Byte::CopyObjectOp:
				return AML::CopyObject::parse(context);
			case AML::Byte::DerefOfOp:
			case AML::Byte::RefOfOp:
				return AML::Reference::parse(context);
			case AML::Byte::IndexOp:
				return AML::Index::parse(context);
			case AML::Byte::NotifyOp:
				return AML::Notify::parse(context);
			case AML::Byte::SizeOfOp:
				return AML::SizeOf::parse(context);
			case AML::Byte::ObjectTypeOp:
				return AML::ObjectType::parse(context);
			case AML::Byte::BreakPointOp: // TODO: support breakpoints?
			case AML::Byte::NoopOp:
				context.aml_data = context.aml_data.slice(1);
				return ParseResult::Success;
			case AML::Byte::ReturnOp:
			{
				context.aml_data = context.aml_data.slice(1);
				auto result = AML::parse_object(context);
				if (result.success())
					return ParseResult(ParseResult::Result::Returned, result.node());
				AML_ERROR("Failed to parse return value for method {}", context.scope);
				return ParseResult::Failure;
			}
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
			auto aml_object = Namespace::root_namespace()->find_object(context.scope, name_string.value(), Namespace::FindMode::Normal);
			if (!aml_object)
			{
				AML_ERROR("NameString {} not found in namespace", name_string.value());
				return ParseResult::Failure;
			}

			auto underlying = aml_object->to_underlying();
			if (aml_object->type != AML::Node::Type::Method && underlying->type != AML::Node::Type::Method)
				return ParseResult(aml_object);

			auto* method = static_cast<AML::Method*>(
				aml_object->type == AML::Node::Type::Method
					? aml_object.ptr()
					: underlying.ptr()
			);

			BAN::Array<BAN::RefPtr<AML::Node>, 7> args;
			for (uint8_t i = 0; i < method->arg_count; i++)
			{
				auto arg_result = AML::parse_object(context);
				if (!arg_result.success() || !arg_result.node())
				{
					AML_ERROR("Failed to parse argument {} for method {}", i, name_string.value());
					return ParseResult::Failure;
				}
				args[i] = arg_result.node();
			}

			auto result = method->invoke_with_sync_stack(
				context.sync_stack,
				args[0],
				args[1],
				args[2],
				args[3],
				args[4],
				args[5],
				args[6]
			);
			if (!result.has_value())
			{
				AML_ERROR("Failed to evaluate {}", name_string.value());
				return ParseResult::Failure;
			}
			if (!result.value())
				return ParseResult::Success;
			return ParseResult(result.value());
		}

		AML_TODO("{2H}", context.aml_data[0]);
		return ParseResult::Failure;
	}

}
