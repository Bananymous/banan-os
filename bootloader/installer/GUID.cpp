#include "GUID.h"

#include <iomanip>
#include <cstring>

std::optional<uint64_t> parse_hex(std::string_view hex_string)
{
	uint64_t result = 0;
	for (char c : hex_string)
	{
		if (!isxdigit(c))
			return {};

		uint8_t nibble = 0;
		if ('0' <= c && c <= '9')
			nibble = c - '0';
		else if ('a' <= c && c <= 'f')
			nibble = c - 'a' + 10;
		else
			nibble = c - 'A' + 10;
		result = (result << 4) | nibble;
	}
	return result;
}

std::optional<GUID> GUID::from_string(std::string_view guid_string)
{
	if (guid_string.size() != 36)
		return {};

	if (guid_string[8] != '-' || guid_string[13] != '-' || guid_string[18] != '-' || guid_string[23] != '-')
		return {};

	auto comp1 = parse_hex(guid_string.substr(0, 8));
	auto comp2 = parse_hex(guid_string.substr(9, 4));
	auto comp3 = parse_hex(guid_string.substr(14, 4));
	auto comp4 = parse_hex(guid_string.substr(19, 4));
	auto comp5 = parse_hex(guid_string.substr(24, 12));

	if (!comp1.has_value() || !comp2.has_value() || !comp3.has_value() || !comp4.has_value() || !comp5.has_value())
		return {};

	GUID result;
	result.component1 = *comp1;
	result.component2 = *comp2;
	result.component3 = *comp3;
	for (int i = 0; i < 2; i++)
		result.component45[i + 0] = *comp4 >> ((2-1) * 8 - i * 8);
	for (int i = 0; i < 6; i++)
		result.component45[i + 2] = *comp5 >> ((6-1) * 8 - i * 8);
	return result;
}

bool GUID::operator==(const GUID& other) const
{
	return std::memcmp(this, &other, sizeof(GUID)) == 0;
}

std::ostream& operator<<(std::ostream& out, const GUID& guid)
{
	auto flags = out.flags();
	out << std::hex << std::setfill('0');
	out << std::setw(8) << guid.component1 << '-';
	out << std::setw(4) << guid.component2 << '-';
	out << std::setw(4) << guid.component3 << '-';

	out << std::setw(2);
	for (int i = 0; i < 2; i++) out << +guid.component45[i];
	out << '-';
	for (int i = 2; i < 8; i++) out << +guid.component45[i];

	out.flags(flags);
	return out;
}
