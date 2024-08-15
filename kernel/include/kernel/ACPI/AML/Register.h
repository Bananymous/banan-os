#pragma once

#include <kernel/ACPI/AML/Node.h>

namespace Kernel::ACPI::AML
{

	struct Register final : public AML::Node
	{
		BAN::RefPtr<AML::Node> value;

		Register();
		Register(BAN::RefPtr<AML::Node> node);

		BAN::RefPtr<AML::Node> convert(uint8_t mask) override;
		BAN::RefPtr<AML::Node> store(BAN::RefPtr<AML::Node> source) override;

		void debug_print(int indent) const override;
	};

}
