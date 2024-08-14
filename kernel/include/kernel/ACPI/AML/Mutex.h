#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/Lock/Mutex.h>
#include <kernel/Timer/Timer.h>

namespace Kernel::ACPI::AML
{

	struct Mutex : public AML::NamedObject
	{
		Kernel::Mutex mutex;
		uint8_t sync_level;

		Mutex(NameSeg name, uint8_t sync_level)
			: NamedObject(Node::Type::Mutex, name)
			, sync_level(sync_level)
		{}

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);

			switch (static_cast<AML::ExtOp>(context.aml_data[1]))
			{
				case AML::ExtOp::MutexOp:
					return parse_mutex(context);
				case AML::ExtOp::AcquireOp:
					return parse_acquire(context);
				case AML::ExtOp::ReleaseOp:
					return parse_release(context);
				default:
					ASSERT_NOT_REACHED();
			}
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Mutex ");
			name.debug_print();
			AML_DEBUG_PRINT(" (SyncLevel: {})", sync_level);
		}

	private:
		static ParseResult parse_mutex(ParseContext& context)
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
			if (!Namespace::root_namespace()->add_named_object(context, name.value(), mutex))
				return ParseResult::Success;

#if AML_DEBUG_LEVEL >= 2
			mutex->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult::Success;
		}

		static ParseResult parse_acquire(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::AcquireOp);
			context.aml_data = context.aml_data.slice(2);

			auto mutex_result = AML::parse_object(context);
			if (!mutex_result.success() || !mutex_result.node() || mutex_result.node()->type != AML::Node::Type::Mutex)
			{
				AML_ERROR("Acquire does not name a valid mutex");
				return ParseResult::Failure;
			}

			auto* mutex = static_cast<AML::Mutex*>(mutex_result.node().ptr());
			if (mutex->sync_level < context.sync_level())
			{
				AML_ERROR("Trying to acquire mutex with lower sync level than current sync level");
				return ParseResult::Failure;
			}

			if (context.aml_data.size() < 2)
			{
				AML_ERROR("Missing timeout value");
				return ParseResult::Failure;
			}
			uint16_t timeout = context.aml_data[0] | (context.aml_data[1] << 8);
			context.aml_data = context.aml_data.slice(2);

			if (timeout >= 0xFFFF)
				mutex->mutex.lock();
			else
			{
				// FIXME: This is a very inefficient way to wait for a mutex
				uint64_t wake_time = SystemTimer::get().ms_since_boot() + timeout;
				while (!mutex->mutex.try_lock())
				{
					if (SystemTimer::get().ms_since_boot() >= wake_time)
						return ParseResult(Integer::Constants::Ones);
					Processor::yield();
				}
			}

			MUST(context.sync_stack.push_back(mutex->sync_level));
			return ParseResult(Integer::Constants::Zero);
		}

		static ParseResult parse_release(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::ReleaseOp);
			context.aml_data = context.aml_data.slice(2);

			auto mutex_result = AML::parse_object(context);
			if (!mutex_result.success() || !mutex_result.node() || mutex_result.node()->type != AML::Node::Type::Mutex)
			{
				AML_ERROR("Release does not name a valid mutex");
				return ParseResult::Failure;
			}

			if (context.sync_stack.empty())
			{
				AML_ERROR("Trying to release mutex without having acquired it");
				return ParseResult::Failure;
			}

			auto* mutex = static_cast<AML::Mutex*>(mutex_result.node().ptr());
			if (mutex->sync_level != context.sync_level())
			{
				AML_ERROR("Trying to release mutex with different sync level than current sync level");
				return ParseResult::Failure;
			}

			mutex->mutex.unlock();
			context.sync_stack.pop_back();

			return ParseResult::Success;
		}

	};

}
