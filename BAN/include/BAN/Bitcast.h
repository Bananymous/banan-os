#pragma once

namespace BAN
{

	template<typename To, typename From>
	constexpr To bit_cast(const From& from)
	{
		return __builtin_bit_cast(To, from);
	}

}
