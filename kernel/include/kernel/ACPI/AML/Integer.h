#pragma once

#include <BAN/Optional.h>
#include <BAN/String.h>
#include <BAN/Vector.h>
#include <kernel/ACPI/AML/Bytes.h>
#include <kernel/ACPI/AML/Node.h>
#include <kernel/ACPI/AML/Utils.h>

namespace Kernel::ACPI::AML
{

	struct Integer final : public AML::Node
	{
		struct Constants
		{
			// Initialized in Namespace::create_root_namespace
			static BAN::RefPtr<Integer> Zero;
			static BAN::RefPtr<Integer> One;
			static BAN::RefPtr<Integer> Ones;
		};

		uint64_t value;
		const bool constant;

		Integer(uint64_t value, bool constant = false);

		BAN::Optional<bool> logical_compare(BAN::RefPtr<AML::Node> node, AML::Byte binaryop);

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override;
		BAN::RefPtr<Node> copy() override;
		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> store_node) override;

		static ParseResult parse(BAN::ConstByteSpan& aml_data);

		void debug_print(int indent) const override;
	};

}
