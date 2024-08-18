#pragma once

#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Reference.h>

namespace Kernel::ACPI::AML
{

	struct ObjectType
	{
		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::ObjectTypeOp);
			context.aml_data = context.aml_data.slice(1);

			auto object_result = AML::parse_object(context);
			if (!object_result.success())
				return ParseResult::Failure;
			auto object = object_result.node()
				? object_result.node()->to_underlying()
				: BAN::RefPtr<AML::Node>();

			if (object && object->type == AML::Node::Type::Reference)
				object = static_cast<AML::Reference*>(object.ptr())->node->to_underlying();

			uint64_t value = 0;
			if (object)
			{
				switch (object->type)
				{
					case AML::Node::Type::None:
					case AML::Node::Type::Alias:
					case AML::Node::Type::Name:
					case AML::Node::Type::PackageElement:
					case AML::Node::Type::Reference:
					case AML::Node::Type::Register:
						ASSERT_NOT_REACHED();
					case AML::Node::Type::Namespace:
						value = 0;
						break;
					case AML::Node::Type::Integer:
						value = 1;
						break;
					case AML::Node::Type::String:
						value = 2;
						break;
					case AML::Node::Type::Buffer:
						value = 3;
						break;
					case AML::Node::Type::Package:
						value = 4;
						break;
					case AML::Node::Type::FieldElement:
					case AML::Node::Type::BankFieldElement:
					case AML::Node::Type::IndexFieldElement:
						value = 5;
						break;
					case AML::Node::Type::Device:
						value = 6;
						break;
					case AML::Node::Type::Event:
						value = 7;
						break;
					case AML::Node::Type::Method:
						value = 8;
						break;
					case AML::Node::Type::Mutex:
						value = 9;
						break;
					case AML::Node::Type::OpRegion:
						value = 10;
						break;
					case AML::Node::Type::PowerResource:
						value = 11;
						break;
					case AML::Node::Type::Processor:
						value = 12;
						break;
					case AML::Node::Type::ThermalZone:
						value = 13;
						break;
					case AML::Node::Type::BufferField:
						value = 14;
						break;
					case AML::Node::Type::Debug:
						value = 16;
						break;
				}
			}

#if AML_DEBUG_LEVEL >= 2
			if (!object)
				AML_DEBUG_PRINTLN("ObjectType { null }");
			else
			{
				AML_DEBUG_PRINTLN("ObjectType {");
				object->debug_print(1);
				AML_DEBUG_PRINTLN("");
			}
#endif

			return ParseResult(MUST(BAN::RefPtr<AML::Integer>::create(value)));
		}
	};

}
