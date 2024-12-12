#pragma once

#include <kernel/ACPI/AML/Node.h>

namespace Kernel::ACPI::AML
{

	BAN::ErrorOr<void> parse_opregion_op(ParseContext& context);
	BAN::ErrorOr<void> parse_field_op(ParseContext& context);
	BAN::ErrorOr<void> parse_index_field_op(ParseContext& context);
	BAN::ErrorOr<void> parse_bank_field_op(ParseContext& context);

	BAN::ErrorOr<Node> convert_from_field_unit(const Node& node, uint8_t conversion, size_t max_length);
	BAN::ErrorOr<void> store_to_field_unit(const Node& source, const Node& target);

}
