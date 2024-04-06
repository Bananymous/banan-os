#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <kernel/ACPI/AML/Bytes.h>

namespace Kernel::ACPI::AML
{

	struct PkgLength
	{
		static BAN::Optional<BAN::ConstByteSpan> parse_package(BAN::ConstByteSpan& span)
		{
			if (span.size() < 1)
				return {};

			uint8_t count = (span[0] >> 6) + 1;
			if (span.size() < count)
				return {};
			if (count > 1 && (span[0] & 0x30))
				return {};

			uint32_t length = span[0] & 0x3F;
			for (uint8_t i = 1; i < count; i++)
				length |= static_cast<uint32_t>(span[i]) << (i * 8 - 4);

			if (span.size() < length)
				return {};

			auto result = span.slice(count, length - count);
			span = span.slice(length);
			return result;
		}
	};

}
