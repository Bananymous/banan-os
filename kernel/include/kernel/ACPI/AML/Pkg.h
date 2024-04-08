#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>

namespace Kernel::ACPI::AML
{

	static BAN::Optional<uint32_t> parse_pkg_length(BAN::ConstByteSpan aml_data)
	{
		if (aml_data.size() < 1)
			return {};

		uint8_t lead_byte = aml_data[0];
		if ((lead_byte & 0xC0) && (lead_byte & 0x30))
			return {};

		uint32_t pkg_length = lead_byte & 0x3F;
		uint8_t byte_count = (lead_byte >> 6) + 1;

		if (aml_data.size() < byte_count)
			return {};

		for (uint8_t i = 1; i < byte_count; i++)
			pkg_length |= aml_data[i] << (i * 8 - 4);

		return pkg_length;
	}

	static void trim_pkg_length(BAN::ConstByteSpan& aml_data)
	{
		ASSERT(aml_data.size() >= 1);
		uint8_t byte_count = (aml_data[0] >> 6) + 1;
		aml_data = aml_data.slice(byte_count);
	}

	static BAN::Optional<BAN::ConstByteSpan> parse_pkg(BAN::ConstByteSpan& aml_data)
	{
		auto pkg_length = parse_pkg_length(aml_data);
		if (!pkg_length.has_value())
			return {};

		auto result = aml_data.slice(0, pkg_length.value());
		trim_pkg_length(result);

		aml_data = aml_data.slice(pkg_length.value());

		return result;
	}

}
