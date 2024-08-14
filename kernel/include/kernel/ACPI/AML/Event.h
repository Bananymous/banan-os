#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Integer.h>
#include <kernel/ACPI/AML/NamedObject.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ThreadBlocker.h>
#include <kernel/Timer/Timer.h>

namespace Kernel::ACPI::AML
{

	struct Event final : public AML::NamedObject
	{
		BAN::Atomic<uint32_t> signal_count { 0 };
		ThreadBlocker thread_blocker;

		Event(NameSeg name)
			: NamedObject(Node::Type::Event, name)
		{}

		BAN::RefPtr<AML::Node> convert(uint8_t) override { return {}; }

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);

			const auto ext_op = static_cast<AML::ExtOp>(context.aml_data[1]);
			switch (ext_op)
			{
				case AML::ExtOp::EventOp:
					return parse_event(context);
				case AML::ExtOp::ResetOp:
				case AML::ExtOp::SignalOp:
				case AML::ExtOp::WaitOp:
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			context.aml_data = context.aml_data.slice(2);

			auto event_result = parse_object(context);
			if (!event_result.success())
				return ParseResult::Failure;

			auto general_node = event_result.node();
			if (!general_node || general_node->type != Node::Type::Event)
			{
				AML_ERROR("Release, Wait or Signal does not name an event");
				return ParseResult::Failure;
			}

			auto* event_node = static_cast<AML::Event*>(general_node.ptr());

			if (ext_op == AML::ExtOp::WaitOp)
			{
				auto timeout_result = parse_object(context);
				if (!timeout_result.success())
					return ParseResult::Failure;

				auto timeout_node = timeout_result.node()
					? timeout_result.node()->convert(AML::Node::ConvInteger)
					: BAN::RefPtr<AML::Node>();
				if (!timeout_node)
				{
					AML_ERROR("Wait timeout does not evaluate to integer");
					return ParseResult::Failure;
				}
				const auto timeout_value = static_cast<AML::Integer*>(timeout_node.ptr())->value;

				const uint64_t start_ms = SystemTimer::get().ms_since_boot();
				while (true)
				{
					auto expected = event_node->signal_count.load();
					while (true)
					{
						if (expected == 0)
							break;
						if (event_node->signal_count.compare_exchange(expected, expected - 1))
							return ParseResult(Integer::Constants::Zero);
					}

					if (timeout_value >= 0xFFFF)
						event_node->thread_blocker.block_indefinite();
					else
					{
						const uint64_t current_ms = SystemTimer::get().ms_since_boot();
						if (current_ms >= start_ms + timeout_value)
							return ParseResult(Integer::Constants::Ones);
						event_node->thread_blocker.block_with_timeout_ms(start_ms + timeout_value - current_ms);
					}
				}

				ASSERT_NOT_REACHED();
			}

			switch (ext_op)
			{
				case AML::ExtOp::ResetOp:
					event_node->signal_count = 0;
					break;
				case AML::ExtOp::SignalOp:
					event_node->signal_count++;
					event_node->thread_blocker.unblock();
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			return ParseResult::Success;
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Event ");
			name.debug_print();
			AML_DEBUG_PRINT(" (Signals: {})", signal_count.load());
		}

	private:
		static ParseResult parse_event(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::EventOp);
			context.aml_data = context.aml_data.slice(2);

			auto name_string = NameString::parse(context.aml_data);
			if (!name_string.has_value())
				return ParseResult::Failure;

			auto event = MUST(BAN::RefPtr<Event>::create(name_string->path.back()));
			if (!Namespace::root_namespace()->add_named_object(context, name_string.value(), event))
				return ParseResult::Success;

#if AML_DEBUG_LEVEL >= 2
			event->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult::Success;
		}
	};

}
