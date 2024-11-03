#pragma once

#include <BAN/Traits.h>

namespace BAN::numbers
{

	template<floating_point T> inline constexpr T e_v      = 2.71828182845904523536;
	template<floating_point T> inline constexpr T log2e_v  = 1.44269504088896340736;
	template<floating_point T> inline constexpr T lge_v    = 0.43429448190325182765;
	template<floating_point T> inline constexpr T lg2_v    = 0.30102999566398119521;
	template<floating_point T> inline constexpr T ln2_v    = 0.69314718055994530942;
	template<floating_point T> inline constexpr T ln10_v   = 2.30258509299404568402;
	template<floating_point T> inline constexpr T pi_v     = 3.14159265358979323846;
	template<floating_point T> inline constexpr T sqrt2_v  = 1.41421356237309504880;
	template<floating_point T> inline constexpr T sqrt3_v  = 1.73205080756887729353;

	inline constexpr double e      = e_v<double>;
	inline constexpr double log2e  = log2e_v<double>;
	inline constexpr double lge    = lge_v<double>;
	inline constexpr double lg2    = lge_v<double>;
	inline constexpr double ln2    = ln2_v<double>;
	inline constexpr double ln10   = ln10_v<double>;
	inline constexpr double pi     = pi_v<double>;
	inline constexpr double sqrt2  = sqrt2_v<double>;
	inline constexpr double sqrt3  = sqrt3_v<double>;

}
