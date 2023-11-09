#include "GUID.h"

#include <iomanip>
#include <cstring>

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
	for (int i = 2; i < 6; i++) out << +guid.component45[i];

	out.flags(flags);
	return out;
}
