#pragma once

#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>

namespace Kernel::ACPI::AML
{

	struct Buffer : public AML::Node
	{
		BAN::Vector<uint8_t> buffer;

		Buffer() : AML::Node(Node::Type::Buffer) {}

		static ParseResult parse(AML::ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 1);
			ASSERT(static_cast<Byte>(context.aml_data[0]) == Byte::BufferOp);
			context.aml_data = context.aml_data.slice(1);

			auto buffer_pkg = AML::parse_pkg(context.aml_data);
			if (!buffer_pkg.has_value())
				return ParseResult::Failure;

			auto buffer_context = context;
			buffer_context.aml_data = buffer_pkg.value();

			auto buffer_size_object = AML::parse_object(buffer_context);
			if (!buffer_size_object.success())
				return ParseResult::Failure;

			auto buffer_size = buffer_size_object.node()->as_integer();
			if (!buffer_size.has_value())
				return ParseResult::Failure;

			uint32_t actual_buffer_size = BAN::Math::max(buffer_size.value(), buffer_context.aml_data.size());

			auto buffer = MUST(BAN::RefPtr<Buffer>::create());
			MUST(buffer->buffer.resize(actual_buffer_size, 0));
			for (uint32_t i = 0; i < buffer_context.aml_data.size(); i++)
					buffer->buffer[i] = buffer_context.aml_data[i];

#if AML_DEBUG_LEVEL >= 2
			buffer->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return ParseResult(buffer);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("Buffer ({} bytes)", buffer.size());
		}
	};

}
