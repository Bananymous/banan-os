#pragma once

#include <BAN/Endianness.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/ParseContext.h>
#include <kernel/ACPI/AML/Pkg.h>
#include <kernel/ACPI/AML/Scope.h>

namespace Kernel::ACPI::AML
{

	struct Processor final : public AML::Scope
	{
		uint8_t id;
		uint32_t pblk_addr;
		uint8_t pblk_len;

		Processor(NameSeg name, uint8_t id, uint32_t pblk_addr, uint8_t pblk_len)
			: Scope(Node::Type::Processor, name)
			, id(id)
			, pblk_addr(pblk_addr)
			, pblk_len(pblk_len)
		{}

		BAN::RefPtr<AML::Node> convert(uint8_t) override { return {}; }

		static ParseResult parse(ParseContext& context)
		{
			ASSERT(context.aml_data.size() >= 2);
			ASSERT(static_cast<AML::Byte>(context.aml_data[0]) == AML::Byte::ExtOpPrefix);
			ASSERT(static_cast<AML::ExtOp>(context.aml_data[1]) == AML::ExtOp::ProcessorOp);
			context.aml_data = context.aml_data.slice(2);

			auto opt_processor_pkg = AML::parse_pkg(context.aml_data);
			if (!opt_processor_pkg.has_value())
				return ParseResult::Failure;
			auto processor_pkg = opt_processor_pkg.value();

			auto name = NameString::parse(processor_pkg);
			if (!name.has_value())
				return ParseResult::Failure;

			if (processor_pkg.size() < 1)
				return ParseResult::Failure;
			uint8_t id = processor_pkg[0];
			processor_pkg = processor_pkg.slice(1);

			if (processor_pkg.size() < 4)
				return ParseResult::Failure;
			uint32_t pblk_addr = BAN::little_endian_to_host<uint32_t>(*reinterpret_cast<const uint32_t*>(processor_pkg.data()));
			processor_pkg = processor_pkg.slice(4);

			if (processor_pkg.size() < 1)
				return ParseResult::Failure;
			uint8_t pblk_len = processor_pkg[0];
			processor_pkg = processor_pkg.slice(1);

			auto processor = MUST(BAN::RefPtr<Processor>::create(name->path.back(), id, pblk_addr, pblk_len));
			if (!Namespace::root_namespace()->add_named_object(context, name.value(), processor))
				return ParseResult::Success;

#if AML_DEBUG_LEVEL >= 2
			processor->debug_print(0);
			AML_DEBUG_PRINTLN("");
#endif

			return processor->enter_context_and_parse_term_list(context, name.value(), processor_pkg);
		}

		virtual void debug_print(int indent) const override
		{
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINTLN("Processor {} (ID: {}, PBlkAddr: 0x{H}, PBlkLen: {}) {", name, id, pblk_addr, pblk_len);
			Namespace::root_namespace()->for_each_child(scope,
				[&](const auto&, const auto& child)
				{
					child->debug_print(indent + 1);
					AML_DEBUG_PRINTLN("");
				}
			);
			AML_DEBUG_PRINT_INDENT(indent);
			AML_DEBUG_PRINT("}");
		}
	};

}
