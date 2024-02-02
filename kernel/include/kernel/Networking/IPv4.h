#pragma once

#include <BAN/Vector.h>

namespace Kernel
{

	BAN::ErrorOr<void> add_ipv4_header(BAN::Vector<uint8_t>&, uint32_t src_ipv4, uint32_t dst_ipv4, uint8_t protocol);

}
