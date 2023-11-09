#pragma once

#include <cstddef>
#include <cstdint>

uint32_t crc32_checksum(const uint8_t* data, std::size_t count);
