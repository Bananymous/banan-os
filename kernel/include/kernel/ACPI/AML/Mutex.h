#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/ParseContext.h>

namespace Kernel::ACPI::AML
{

	struct Mutex : public AML::NamedObject
	{
		uint8_t sync_level;

		Mutex(NameSeg name, uint8_t sync_level)
			: NamedObject(Node::Type::Mutex, name)
			, sync_level(sync_level)
		{}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::MutexOp);
			context.aml_data = context.aml_data.slice(2);

			auto name = NameString::parse(context.aml_data);
			if (!name.has_value())
				return ParseResult::Failure;

			if (context.aml_data.size() < 1)
				return ParseResult::Failure;
			auto sync_level = context.aml_data[0];
			context.aml_data = context.aml_data.slice(1);

			if (sync_level & 0xF0)
			{
				AML_ERROR("Invalid sync level {}", sync_level);
				return ParseResult::Failure;
			}

			auto mutex = MUST(BAN::RefPtr<Mutex>::create(name->path.back(), sync_level));
			if (!context.root_namespace->add_named_object(context, name.value(), mutex))
				return ParseResult::Failure;

#if AML_DEBUG_LEVEL >= 2
			mutex->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult::Success;
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Mutex ");
			name.debug_print();
			AML_DEBUG_PRINT(" (SyncLevel: {})", sync_level);
		}
	};

}
