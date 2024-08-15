#pragma once

#include <BAN/Function.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/Namespace.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Register.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	struct Method final : public AML::Scope
	{
		Kernel::Mutex mutex;
		uint8_t arg_count;
		bool serialized;
		uint8_t sync_level;

		BAN::Function<BAN::RefPtr<AML::Node>(ParseContext&)> override_function;
		BAN::ConstByteSpan term_list;

		Method(AML::NameSeg name, uint8_t arg_count, bool serialized, uint8_t sync_level)
			: AML::Scope(Node::Type::Method, name)
			, arg_count(arg_count)
			, serialized(serialized)
			, sync_level(sync_level)
		{}

		BAN::RefPtr<AML::Node> convert(uint8_t) override { return {}; }

		static ParseResult parse(AML::ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::MethodOp);
			context.aml_data = context.aml_data.slice(1);

			auto method_pkg = AML::parse_pkg(context.aml_data);
			if (!method_pkg.has_value())
				return ParseResult::Failure;

			auto name_string = AML::NameString::parse(method_pkg.value());
			if (!name_string.has_value())
				return ParseResult::Failure;

			if (method_pkg->size() < 1)
				return ParseResult::Failure;
			auto method_flags = method_pkg.value()[0];
			method_pkg = method_pkg.value().slice(1);

			auto method = MUST(BAN::RefPtr<Method>::create(
				name_string.value().path.back(),
				method_flags & 0x07,
				(method_flags >> 3) & 0x01,
				method_flags >> 4
			));
			if (!Namespace::root_namespace()->add_named_object(context, name_string.value(), method))
				return ParseResult::Success;
			method->term_list = method_pkg.value();

#if AML_DEBUG_LEVEL >= 2
			method->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult::Success;
		}

		BAN::Optional<BAN::RefPtr<AML::Node>> invoke(
			BAN::RefPtr<AML::Node> arg0 = {},
			BAN::RefPtr<AML::Node> arg1 = {},
			BAN::RefPtr<AML::Node> arg2 = {},
			BAN::RefPtr<AML::Node> arg3 = {},
			BAN::RefPtr<AML::Node> arg4 = {},
			BAN::RefPtr<AML::Node> arg5 = {},
			BAN::RefPtr<AML::Node> arg6 = {}
		)
		{
			BAN::Vector<uint8_t> sync_stack;
			return invoke_with_sync_stack(sync_stack, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
		}

		BAN::Optional<BAN::RefPtr<AML::Node>> invoke_with_sync_stack(
			BAN::Vector<uint8_t>& current_sync_stack,
			BAN::RefPtr<AML::Node> arg0 = {},
			BAN::RefPtr<AML::Node> arg1 = {},
			BAN::RefPtr<AML::Node> arg2 = {},
			BAN::RefPtr<AML::Node> arg3 = {},
			BAN::RefPtr<AML::Node> arg4 = {},
			BAN::RefPtr<AML::Node> arg5 = {},
			BAN::RefPtr<AML::Node> arg6 = {}
		)
		{
			if (serialized && !current_sync_stack.empty() && sync_level < current_sync_stack.back())
			{
				AML_ERROR("Trying to evaluate method {} with lower sync level than current sync level", scope);
				return {};
			}

			ParseContext context;
			context.aml_data = term_list;
			context.scope = scope;
			context.method_args[0] = MUST(BAN::RefPtr<AML::Register>::create(arg0));
			context.method_args[1] = MUST(BAN::RefPtr<AML::Register>::create(arg1));
			context.method_args[2] = MUST(BAN::RefPtr<AML::Register>::create(arg2));
			context.method_args[3] = MUST(BAN::RefPtr<AML::Register>::create(arg3));
			context.method_args[4] = MUST(BAN::RefPtr<AML::Register>::create(arg4));
			context.method_args[5] = MUST(BAN::RefPtr<AML::Register>::create(arg5));
			context.method_args[6] = MUST(BAN::RefPtr<AML::Register>::create(arg6));
			context.sync_stack = BAN::move(current_sync_stack);
			for (auto& local : context.method_locals)
				local = MUST(BAN::RefPtr<AML::Register>::create());

			if (serialized)
			{
				mutex.lock();
				MUST(context.sync_stack.push_back(sync_level));
			}

#if AML_DEBUG_LEVEL >= 2
			AML_DEBUG_PRINTLN("Evaluating {}", scope);
#endif

			BAN::Optional<BAN::RefPtr<AML::Node>> return_value = BAN::RefPtr<AML::Node>();

			if (override_function)
				return_value = override_function(context);
			else
			{
				while (context.aml_data.size() > 0)
				{
					auto parse_result = AML::parse_object(context);
					if (parse_result.returned())
					{
						return_value = parse_result.node();
						break;
					}
					if (!parse_result.success())
					{
						AML_ERROR("Method {} evaluate failed", scope);
						return_value = {};
						break;
					}
				}
			}

			if (return_value.has_value() && return_value.value() && return_value.value()->type == AML::Node::Type::Register)
				return_value.value() = static_cast<AML::Register*>(return_value.value().ptr())->value;

			while (!context.created_objects.empty())
			{
				Namespace::root_namespace()->remove_named_object(context.created_objects.back());
				context.created_objects.pop_back();
			}

			if (serialized)
			{
				context.sync_stack.pop_back();
				mutex.unlock();
			}

			current_sync_stack = BAN::move(context.sync_stack);

			return return_value;
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Method ");
			name.debug_print();
			AML_DEBUG_PRINTLN("({} args, {}Serialized, 0x{H}) {", arg_count, serialized ? "" : "Not", sync_level);
			AML_DEBUG_PRINT_INDENT(indent + 1);
			AML_DEBUG_PRINTLN("TermList: {} bytes", term_list.size());
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
