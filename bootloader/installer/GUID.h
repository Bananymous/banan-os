#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <string_view>

struct GUID
{
	static std::optional<GUID> from_string(std::string_view);

	uint32_t component1;
	uint16_t component2;
	uint16_t component3;
	// last 2 components are combined so no packed needed
	uint8_t component45[8];

	bool operator==(const GUID& other) const;
};

std::ostream& operator<<(std::ostream& out, const GUID& guid);

// unused		00000000-0000-0000-0000-000000000000
static constexpr GUID unused_guid = {
	0x00000000,
	0x0000,
	0x0000,
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
};

// bios boot	21686148-6449-6E6F-744E-656564454649
static constexpr GUID bios_boot_guid = {
	0x21686148,
	0x6449,
	0x6E6F,
	{ 0x74, 0x4E, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 }
};
